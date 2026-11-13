#pragma once

#include <DiaObservation/Profile/ScopeRecord.h>

#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		class ProfileFileSink;

		class Profiler
		{
		public:
			static Profiler& Instance();

			bool Start(const char* profileFilePath, ProfileCategory activeMask,
			           const char* sessionId, int64_t epochOffsetNs);
			void Stop();

			void BeginFrame();   // increments frame counter; signals drain of previous frame records
			void EndFrame();     // no-op in v1, reserved for GPU fence integration
			uint32_t GetCurrentFrame() const;

			void            SetActiveMask(ProfileCategory mask);
			ProfileCategory ActiveMask() const;

			void RegisterThreadScopeBuffer();
			void UnregisterThreadScopeBuffer();

			// Called by ScopedZone only
			uint64_t OnScopeOpen(const Dia::Core::StringCRC& name, ProfileCategory category,
			                     uint32_t& outFrameNumber, uint64_t& outParentScopeId,
			                     uint32_t subject = 0);
			void     OnScopeClose(uint64_t scopeId, const Dia::Core::StringCRC& name,
			                      ProfileCategory category, uint64_t parentScopeId,
			                      uint64_t startNs, uint64_t durationNs,
			                      uint32_t frameNumber, uint32_t threadId);

			// Snapshot open scopes (for crash handler)
			struct OpenScopeEntry
			{
				Dia::Core::StringCRC name;
				uint64_t             startUnixNano;
				uint32_t             frameNumber;
				uint32_t             threadId;
				uint32_t             subject;
			};
			unsigned int GetOpenScopesSnapshot(OpenScopeEntry* out, unsigned int maxOut) const;

			// Subject of the innermost open scope on the calling thread (0 if none).
			static uint32_t CurrentSubject();

			// Subject name table (fixed capacity). Registration is off the hot path.
			static const unsigned int kMaxSubjectNames = 256;
			static const unsigned int kSubjectNameLen  = 64;
			static bool RegisterSubjectName(uint32_t subject, const char* name);   // false if table full / invalid
			// Writes the registered name, or "0x%08x" when unknown. Returns buf (NUL-terminated).
			static const char* ResolveSubjectName(uint32_t subject, char* buf, size_t bufSize);

			// Copies not-yet-drained closed records of the CALLING thread (oldest first). Test/diagnostic aid.
			unsigned int PeekThreadClosedRecords(ScopeRecord* out, unsigned int maxOut) const;

			bool IsStarted() const { return mStarted.load(std::memory_order_acquire); }

		private:
			Profiler();
			~Profiler();
			Profiler(const Profiler&) = delete;
			Profiler& operator=(const Profiler&) = delete;

			void DrainLoop();
			void DrainAllRings(uint32_t beforeFrame);

			struct ThreadRingRef
			{
				ScopeRecord*               closedRing;
				std::atomic<unsigned int>* head;
				std::atomic<unsigned int>* tail;
				uint32_t*                  openCount;
				uint64_t*                  openScopeIds;    // parallel arrays of open stack
				uint64_t*                  openStartNs;
				Dia::Core::StringCRC*      openNames;
				uint32_t*                  openFrameNumbers;
				uint32_t*                  openSubjects;
				uint32_t                   threadId;
			};

			static const unsigned int kMaxThreadRings = 16;
			ThreadRingRef            mThreadRings[kMaxThreadRings];
			unsigned int             mThreadRingCount;
			mutable std::mutex       mRegistryMutex;

			ProfileFileSink*              mSink;
			std::atomic<ProfileCategory>  mActiveMask;
			std::atomic<uint32_t>         mCurrentFrame;

			std::thread       mDrainThread;
			std::atomic<bool> mDrainRunning;
			std::atomic<bool> mStarted;
			std::atomic<bool> mFrameAdvanced;
		};
	}
} // namespace Observation
} // namespace Dia
