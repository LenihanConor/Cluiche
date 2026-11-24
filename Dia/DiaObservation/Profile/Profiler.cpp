#include "DiaObservation/Profile/Profiler.h"
#include "DiaObservation/Profile/ProfileFileSink.h"
#include "DiaObservation/Log/DiaLog.h"

#include <DiaCore/Core/Assert.h>

#include <chrono>
#include <cstring>
#include <climits>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Dia
{
	namespace Observation { namespace Profile
	{
		static const unsigned int kOpenStackCapacity  = 64;
		static const unsigned int kClosedRingCapacity = 512;

		// --- Thread-local state ---

		static thread_local uint64_t             tScopeCounter = 0;
		static thread_local uint64_t             tOpenStack[kOpenStackCapacity];        // scope IDs of open scopes (LIFO)
		static thread_local uint64_t             tOpenStartNs[kOpenStackCapacity];      // start times parallel to stack
		static thread_local uint64_t             tOpenParentId[kOpenStackCapacity];     // parent IDs parallel to stack
		static thread_local Dia::Core::StringCRC tOpenName[kOpenStackCapacity];
		static thread_local ProfileCategory      tOpenCategory[kOpenStackCapacity];
		static thread_local uint32_t             tOpenFrameNumber[kOpenStackCapacity];
		static thread_local uint32_t             tOpenSubject[kOpenStackCapacity];      // inherited/explicit subject per open scope
		static thread_local unsigned int         tOpenCount     = 0;
		static thread_local ScopeRecord          tClosedRing[kClosedRingCapacity];
		static thread_local std::atomic<unsigned int> tClosedHead{0};
		static thread_local std::atomic<unsigned int> tClosedTail{0};
		static thread_local ProfileJobRecord     tJobRing[Profiler::kJobRingCapacity];
		static thread_local std::atomic<unsigned int> tJobHead{0};
		static thread_local std::atomic<unsigned int> tJobTail{0};
		static thread_local bool                 tRegistered    = false;
		static std::atomic<uint64_t>             sDroppedJobRecords{0};

		// --- Profiler singleton ---

		Profiler& Profiler::Instance()
		{
			static Profiler sInstance;
			return sInstance;
		}

		Profiler::Profiler()
			: mThreadRingCount(0)
			, mSink(nullptr)
			, mActiveMask(Category::kNone)
			, mCurrentFrame(0)
			, mDrainRunning(false)
			, mStarted(false)
			, mFrameAdvanced(false)
		{
			std::memset(mThreadRings, 0, sizeof(mThreadRings));
		}

		Profiler::~Profiler()
		{
			Stop();
		}

		bool Profiler::Start(const char* profileFilePath, ProfileCategory activeMask,
		                     const char* sessionId, int64_t epochOffsetNs)
		{
			if (mStarted.load(std::memory_order_acquire))
				return false;

			// Build the final json path next to the jsonl path
			char finalPath[512];
			std::memset(finalPath, 0, sizeof(finalPath));
			if (profileFilePath)
			{
				strncpy_s(finalPath, profileFilePath, sizeof(finalPath) - 1);
				// Replace ".jsonl" suffix with "-final.json", or append "-final.json"
				size_t len = strlen(finalPath);
				const char* suffix = ".jsonl";
				size_t suffixLen = strlen(suffix);
				if (len >= suffixLen && strcmp(finalPath + len - suffixLen, suffix) == 0)
					strncpy_s(finalPath + len - suffixLen, sizeof(finalPath) - (len - suffixLen), "-final.json", sizeof(finalPath) - (len - suffixLen) - 1);
				else
					strncat_s(finalPath, "-final.json", sizeof(finalPath) - len - 1);
			}

			mSink = new ProfileFileSink(profileFilePath, finalPath, sessionId, epochOffsetNs);
			if (!mSink->IsOpen())
			{
				delete mSink;
				mSink = nullptr;
				return false;
			}

			mActiveMask.store(activeMask, std::memory_order_relaxed);
			mCurrentFrame.store(0, std::memory_order_release);
			mFrameAdvanced.store(false, std::memory_order_release);

			mStarted.store(true, std::memory_order_release);

			mDrainRunning.store(true, std::memory_order_release);
			mDrainThread = std::thread(&Profiler::DrainLoop, this);
#ifdef _WIN32
			SetThreadDescription(mDrainThread.native_handle(), L"DiaObservation::Profile::Drain");
#endif

			return true;
		}

		void Profiler::Stop()
		{
			if (!mStarted.exchange(false, std::memory_order_acq_rel))
				return;

			mDrainRunning.store(false, std::memory_order_release);

			if (mDrainThread.joinable())
				mDrainThread.join();

			// Final drain: flush all remaining records regardless of frame
			DrainAllRings(UINT32_MAX);

			{
				std::lock_guard<std::mutex> lock(mRegistryMutex);   // workers flush into mSink under this lock
				if (mSink)
				{
					mSink->WriteFinalJson();
					delete mSink;
					mSink = nullptr;
				}
			}
		}

		void Profiler::BeginFrame()
		{
			mCurrentFrame.fetch_add(1, std::memory_order_release);
			mFrameAdvanced.store(true, std::memory_order_release);
		}

		void Profiler::EndFrame()
		{
			// No-op in v1; reserved for GPU fence integration
		}

		uint32_t Profiler::GetCurrentFrame() const
		{
			return mCurrentFrame.load(std::memory_order_relaxed);
		}

		void Profiler::SetActiveMask(ProfileCategory mask)
		{
			mActiveMask.store(mask, std::memory_order_relaxed);
		}

		ProfileCategory Profiler::ActiveMask() const
		{
			return mActiveMask.load(std::memory_order_relaxed);
		}

		void Profiler::RegisterThreadScopeBuffer()
		{
			if (tRegistered)
				return;

			tScopeCounter = 0;
			tOpenCount    = 0;
			tClosedHead.store(0, std::memory_order_relaxed);
			tClosedTail.store(0, std::memory_order_relaxed);
			tJobHead.store(0, std::memory_order_relaxed);
			tJobTail.store(0, std::memory_order_relaxed);

			uint32_t thisThreadId = 0;
#ifdef _WIN32
			thisThreadId = static_cast<uint32_t>(GetCurrentThreadId());
#endif

			{
				std::lock_guard<std::mutex> lock(mRegistryMutex);
				if (mThreadRingCount >= kMaxThreadRings)
				{
					DIA_LOG_WARNING("Profile", "Profiler: max thread ring count (%u) exceeded — thread will not emit profile scopes", kMaxThreadRings);
					return;
				}

				ThreadRingRef& ref = mThreadRings[mThreadRingCount];
				ref.closedRing      = tClosedRing;
				ref.head            = &tClosedHead;
				ref.tail            = &tClosedTail;
				ref.openCount       = &tOpenCount;
				ref.openScopeIds    = tOpenStack;
				ref.openStartNs     = tOpenStartNs;
				ref.openNames       = tOpenName;
				ref.openFrameNumbers = tOpenFrameNumber;
				ref.openSubjects    = tOpenSubject;
				ref.jobRing         = tJobRing;
				ref.jobHead         = &tJobHead;
				ref.jobTail         = &tJobTail;
				ref.threadId        = thisThreadId;
				++mThreadRingCount;
			}

			tRegistered = true;
		}

		void Profiler::RegisterNamedThreadScopeBuffer(const char* threadName)
		{
			RegisterThreadScopeBuffer();
			if (threadName && threadName[0] != '\0')
				RegisterThreadName(CurrentThreadId(), threadName);
		}

		bool Profiler::IsThreadScopeBufferRegistered() const
		{
			return tRegistered;
		}

		void Profiler::UnregisterThreadScopeBuffer()
		{
			if (!tRegistered)
				return;

			{
				std::lock_guard<std::mutex> lock(mRegistryMutex);
				for (unsigned int i = 0; i < mThreadRingCount; ++i)
				{
					if (mThreadRings[i].closedRing == tClosedRing)
					{
						// Flush this thread's undrained records before the ring goes away (worker shutdown).
						if (mSink)
							DrainRing(mThreadRings[i], UINT32_MAX);
						mThreadRings[i] = mThreadRings[mThreadRingCount - 1];
						std::memset(&mThreadRings[mThreadRingCount - 1], 0, sizeof(ThreadRingRef));
						--mThreadRingCount;
						break;
					}
				}
			}

			tRegistered   = false;
			tOpenCount    = 0;
			tScopeCounter = 0;
			tClosedHead.store(0, std::memory_order_relaxed);
			tClosedTail.store(0, std::memory_order_relaxed);
			tJobHead.store(0, std::memory_order_relaxed);
			tJobTail.store(0, std::memory_order_relaxed);
		}

		void Profiler::RecordJob(ProfileJobRecord record)
		{
			if (!tRegistered || !mStarted.load(std::memory_order_acquire)
			    || mActiveMask.load(std::memory_order_relaxed) == Category::kNone)
				return;

			const unsigned int head     = tJobHead.load(std::memory_order_relaxed);
			const unsigned int tail     = tJobTail.load(std::memory_order_acquire);
			const unsigned int nextHead = (head + 1) % kJobRingCapacity;
			if (nextHead == tail)
			{
				sDroppedJobRecords.fetch_add(1, std::memory_order_relaxed);   // full: drop newest, never block
				return;
			}

			record.threadId    = CurrentThreadId();
			record.frameNumber = mCurrentFrame.load(std::memory_order_relaxed);
			tJobRing[head]     = record;
			tJobHead.store(nextHead, std::memory_order_release);
		}

		uint64_t Profiler::DroppedJobRecords()
		{
			return sDroppedJobRecords.load(std::memory_order_relaxed);
		}

		unsigned int Profiler::PeekThreadJobRecordCount() const
		{
			const unsigned int head = tJobHead.load(std::memory_order_acquire);
			const unsigned int tail = tJobTail.load(std::memory_order_acquire);
			return (head + kJobRingCapacity - tail) % kJobRingCapacity;
		}

		uint64_t Profiler::OnScopeOpen(const Dia::Core::StringCRC& name, ProfileCategory category,
		                                uint32_t& outFrameNumber, uint64_t& outParentScopeId,
		                                uint32_t subject)
		{
			if (!tRegistered || !mStarted.load(std::memory_order_acquire))
				return 0;

			uint64_t scopeId = ++tScopeCounter;
			uint64_t parentScopeId = (tOpenCount > 0) ? tOpenStack[tOpenCount - 1] : 0;
			uint32_t frameNumber   = mCurrentFrame.load(std::memory_order_relaxed);

#ifdef DEBUG
			DIA_ASSERT(tOpenCount < kOpenStackCapacity, "Profile open stack overflow — max depth is %u", kOpenStackCapacity);
#else
			if (tOpenCount >= kOpenStackCapacity)
				return 0; // Truncate silently in release
#endif

			tOpenStack[tOpenCount]        = scopeId;
			tOpenStartNs[tOpenCount]      = 0; // Caller (ScopedZone) sets start time before calling
			tOpenParentId[tOpenCount]     = parentScopeId;
			tOpenName[tOpenCount]         = name;
			tOpenCategory[tOpenCount]     = category;
			tOpenFrameNumber[tOpenCount]  = frameNumber;
			tOpenSubject[tOpenCount]      = (subject != 0) ? subject : ((tOpenCount > 0) ? tOpenSubject[tOpenCount - 1] : 0u);
			++tOpenCount;

			outFrameNumber    = frameNumber;
			outParentScopeId  = parentScopeId;

			return scopeId;
		}

		void Profiler::OnScopeClose(uint64_t scopeId, const Dia::Core::StringCRC& name,
		                             ProfileCategory category, uint64_t parentScopeId,
		                             uint64_t startNs, uint64_t durationNs,
		                             uint32_t frameNumber, uint32_t threadId)
		{
			if (!tRegistered)
				return;

			uint32_t subject = 0;
			if (tOpenCount > 0)
			{
				subject = tOpenSubject[tOpenCount - 1];
				--tOpenCount;
			}

			// Write ScopeRecord to closed ring (drop-oldest on overflow)
			unsigned int head     = tClosedHead.load(std::memory_order_relaxed);
			unsigned int tail     = tClosedTail.load(std::memory_order_relaxed);

			ScopeRecord& record   = tClosedRing[head];
			record.name           = name;
			record.category       = category;
			record.scopeId        = scopeId;
			record.parentScopeId  = parentScopeId;
			record.startUnixNano  = startNs; // Profiler::DrainAllRings adds epochOffset via sink
			record.durationNs     = durationNs;
			record.frameNumber    = frameNumber;
			record.threadId       = threadId;
			record.subject        = subject;

			unsigned int nextHead = (head + 1) % kClosedRingCapacity;
			if (nextHead == tail)
			{
				// Ring full — drop oldest
				tClosedTail.store((tail + 1) % kClosedRingCapacity, std::memory_order_release);
			}
			tClosedHead.store(nextHead, std::memory_order_release);
		}

		unsigned int Profiler::GetOpenScopesSnapshot(OpenScopeEntry* out, unsigned int maxOut) const
		{
			if (!out || maxOut == 0)
				return 0;

			std::lock_guard<std::mutex> lock(mRegistryMutex);

			unsigned int total = 0;
			for (unsigned int i = 0; i < mThreadRingCount && total < maxOut; ++i)
			{
				const ThreadRingRef& ref = mThreadRings[i];
				unsigned int openCount = *ref.openCount;
				for (unsigned int j = 0; j < openCount && total < maxOut; ++j)
				{
					out[total].name         = ref.openNames[j];
					out[total].startUnixNano = ref.openStartNs[j];
					out[total].frameNumber  = ref.openFrameNumbers[j];
					out[total].threadId     = ref.threadId;
					out[total].subject      = ref.openSubjects[j];
					++total;
				}
			}

			return total;
		}

		uint32_t Profiler::CurrentSubject()
		{
			return (tOpenCount > 0) ? tOpenSubject[tOpenCount - 1] : 0u;
		}

		namespace
		{
			struct SubjectEntry { uint32_t value; char name[Profiler::kSubjectNameLen]; };
			SubjectEntry sSubjects[Profiler::kMaxSubjectNames];
			unsigned int sSubjectCount = 0;
			std::mutex   sSubjectMutex;
		}

		bool Profiler::RegisterSubjectName(uint32_t subject, const char* name)
		{
			if (subject == 0 || name == nullptr)
				return false;

			std::lock_guard<std::mutex> lock(sSubjectMutex);
			SubjectEntry* entry = nullptr;
			for (unsigned int i = 0; i < sSubjectCount; ++i)
			{
				if (sSubjects[i].value == subject)
				{
					entry = &sSubjects[i];
					break;
				}
			}
			if (!entry)
			{
				if (sSubjectCount >= kMaxSubjectNames)
					return false;
				entry = &sSubjects[sSubjectCount++];
				entry->value = subject;
			}
			strncpy_s(entry->name, kSubjectNameLen, name, _TRUNCATE);
			return true;
		}

		namespace
		{
			struct ThreadNameEntry { uint32_t id; char name[Profiler::kThreadNameLen]; };
			ThreadNameEntry sThreadNames[Profiler::kMaxThreadNames];
			unsigned int    sThreadNameCount = 0;
			std::mutex      sThreadNameMutex;
		}

		uint32_t Profiler::CurrentThreadId()
		{
			return static_cast<uint32_t>(GetCurrentThreadId());
		}

		bool Profiler::RegisterThreadName(uint32_t threadId, const char* name)
		{
			if (name == nullptr)
				return false;

			std::lock_guard<std::mutex> lock(sThreadNameMutex);
			ThreadNameEntry* entry = nullptr;
			for (unsigned int i = 0; i < sThreadNameCount; ++i)
			{
				if (sThreadNames[i].id == threadId)
				{
					entry = &sThreadNames[i];
					break;
				}
			}
			if (!entry)
			{
				if (sThreadNameCount >= kMaxThreadNames)
					return false;
				entry = &sThreadNames[sThreadNameCount++];
				entry->id = threadId;
			}
			strncpy_s(entry->name, kThreadNameLen, name, _TRUNCATE);
			return true;
		}

		bool Profiler::ResolveThreadName(uint32_t threadId, char* buf, size_t bufSize)
		{
			if (!buf || bufSize == 0)
				return false;
			std::lock_guard<std::mutex> lock(sThreadNameMutex);
			for (unsigned int i = 0; i < sThreadNameCount; ++i)
			{
				if (sThreadNames[i].id == threadId)
				{
					strncpy_s(buf, bufSize, sThreadNames[i].name, _TRUNCATE);
					return true;
				}
			}
			return false;
		}

		void Profiler::ClearSubjectNames()
		{
			std::lock_guard<std::mutex> lock(sSubjectMutex);
			sSubjectCount = 0;
		}

		void Profiler::ClearThreadNames()
		{
			std::lock_guard<std::mutex> lock(sThreadNameMutex);
			sThreadNameCount = 0;
		}

		const char* Profiler::ResolveSubjectName(uint32_t subject, char* buf, size_t bufSize)
		{
			if (!buf || bufSize == 0)
				return buf;

			{
				std::lock_guard<std::mutex> lock(sSubjectMutex);
				for (unsigned int i = 0; i < sSubjectCount; ++i)
				{
					if (sSubjects[i].value == subject)
					{
						strncpy_s(buf, bufSize, sSubjects[i].name, _TRUNCATE);
						return buf;
					}
				}
			}
			snprintf(buf, bufSize, "0x%08x", subject);
			return buf;
		}

		unsigned int Profiler::PeekThreadClosedRecords(ScopeRecord* out, unsigned int maxOut) const
		{
			if (!out || maxOut == 0)
				return 0;
			unsigned int tail = tClosedTail.load(std::memory_order_acquire);
			unsigned int head = tClosedHead.load(std::memory_order_acquire);
			unsigned int n = 0;
			while (tail != head && n < maxOut)
			{
				out[n++] = tClosedRing[tail];
				tail = (tail + 1) % kClosedRingCapacity;
			}
			return n;
		}

		void Profiler::DrainLoop()
		{
			while (mDrainRunning.load(std::memory_order_acquire))
			{
				if (mFrameAdvanced.load(std::memory_order_acquire))
				{
					uint32_t currentFrame = mCurrentFrame.load(std::memory_order_acquire);
					DrainAllRings(currentFrame);
					mFrameAdvanced.store(false, std::memory_order_release);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		void Profiler::DrainAllRings(uint32_t beforeFrame)
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			for (unsigned int i = 0; i < mThreadRingCount; ++i)
				DrainRing(mThreadRings[i], beforeFrame);
		}

		void Profiler::DrainRing(ThreadRingRef& ref, uint32_t beforeFrame)
		{
			if (ref.closedRing == nullptr)
				return;

			unsigned int tail = ref.tail->load(std::memory_order_acquire);
			unsigned int head = ref.head->load(std::memory_order_acquire);

			while (tail != head)
			{
				const ScopeRecord& record = ref.closedRing[tail];

				// Only drain records from frames that are fully done
				if (record.frameNumber >= beforeFrame)
					break;

				if (mSink)
					mSink->OnRecord(record);

				tail = (tail + 1) % kClosedRingCapacity;
			}

			ref.tail->store(tail, std::memory_order_release);

			unsigned int jTail = ref.jobTail->load(std::memory_order_acquire);
			const unsigned int jHead = ref.jobHead->load(std::memory_order_acquire);
			while (jTail != jHead)
			{
				const ProfileJobRecord& job = ref.jobRing[jTail];
				if (job.frameNumber >= beforeFrame)
					break;
				if (mSink)
					mSink->OnJobRecord(job);
				jTail = (jTail + 1) % kJobRingCapacity;
			}
			ref.jobTail->store(jTail, std::memory_order_release);
		}
	}
} // namespace Observation
} // namespace Dia
