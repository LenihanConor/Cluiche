#include "DiaObservation/Trace/Tracer.h"
#include "DiaObservation/Trace/TraceFileSink.h"
#include "DiaObservation/Trace/ITraceSink.h"
#include "DiaObservation/Session/ScenarioStepStack.h"
#include "DiaObservation/Log/DiaLog.h"

#include <chrono>
#include <random>
#include <cstring>
#include <cassert>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Dia
{
	namespace Observation { namespace Trace
	{
		static const unsigned int kOpenStackCapacity = 64;
		static const unsigned int kClosedRingCapacity = 256;

		static thread_local std::mt19937_64 tRng;
		static thread_local uint64_t tCurrentTraceId = 0;
		static thread_local SpanRecord tOpenStack[kOpenStackCapacity];
		static thread_local unsigned int tOpenCount = 0;
		static thread_local SpanRecord tClosedRing[kClosedRingCapacity];
		static thread_local std::atomic<unsigned int> tClosedHead{0};
		static thread_local std::atomic<unsigned int> tClosedTail{0};
		static thread_local bool tRegistered = false;

		Tracer& Tracer::Instance()
		{
			static Tracer sInstance;
			return sInstance;
		}

		Tracer::Tracer()
			: mThreadRingCount(0)
			, mSink(nullptr)
			, mSessionSeed(0)
			, mTraceSinkCount(0)
			, mDrainRunning(false)
			, mStarted(false)
		{
			std::memset(mThreadRings, 0, sizeof(mThreadRings));
			std::memset(mTraceSinks, 0, sizeof(mTraceSinks));
		}

		Tracer::~Tracer()
		{
			Stop();
		}

		bool Tracer::Start(const char* traceFilePath, const char* sessionId, int64_t epochOffsetNs)
		{
			if (mStarted.load(std::memory_order_acquire))
				return false;

			mSink = new TraceFileSink(traceFilePath, sessionId, epochOffsetNs);
			if (!mSink->IsOpen())
			{
				delete mSink;
				mSink = nullptr;
				return false;
			}

			// Generate session seed for RNG seeding
			auto now = std::chrono::steady_clock::now();
			mSessionSeed = static_cast<uint64_t>(now.time_since_epoch().count());

			mStarted.store(true, std::memory_order_release);

			mDrainRunning.store(true, std::memory_order_release);
			mDrainThread = std::thread(&Tracer::DrainLoop, this);
#ifdef _WIN32
			SetThreadDescription(mDrainThread.native_handle(), L"DiaObservation::Trace::Drain");
#endif

			return true;
		}

		void Tracer::Stop()
		{
			if (!mStarted.exchange(false, std::memory_order_acq_rel))
				return;

			mDrainRunning.store(false, std::memory_order_release);

			if (mDrainThread.joinable())
				mDrainThread.join();

			// Final drain sweep
			DrainAllRings();

			if (mSink)
			{
				delete mSink;
				mSink = nullptr;
			}
		}

		void Tracer::RegisterThreadSpanBuffer()
		{
			if (tRegistered)
				return;

			// Seed the thread-local RNG
			auto now = std::chrono::steady_clock::now();
			uint64_t seed = static_cast<uint64_t>(now.time_since_epoch().count());
#ifdef _WIN32
			seed ^= static_cast<uint64_t>(GetCurrentThreadId());
#endif
			seed ^= mSessionSeed;
			tRng.seed(seed);

			tOpenCount = 0;
			tClosedHead.store(0, std::memory_order_relaxed);
			tClosedTail.store(0, std::memory_order_relaxed);
			tCurrentTraceId = 0;

			{
				std::lock_guard<std::mutex> lock(mRegistryMutex);
				if (mThreadRingCount < kMaxThreadRings)
				{
					mThreadRings[mThreadRingCount].closedRing = tClosedRing;
					mThreadRings[mThreadRingCount].head = &tClosedHead;
					mThreadRings[mThreadRingCount].tail = &tClosedTail;
					++mThreadRingCount;
				}
				else
				{
					DIA_LOG_WARNING("Trace", "Tracer: max thread ring count (%u) exceeded — thread will not emit spans", kMaxThreadRings);
					tRegistered = false;
					return;
				}
			}

			tRegistered = true;
		}

		void Tracer::UnregisterThreadSpanBuffer()
		{
			if (!tRegistered)
				return;

			{
				std::lock_guard<std::mutex> lock(mRegistryMutex);
				for (unsigned int i = 0; i < mThreadRingCount; ++i)
				{
					if (mThreadRings[i].closedRing == tClosedRing)
					{
						mThreadRings[i] = mThreadRings[mThreadRingCount - 1];
						std::memset(&mThreadRings[mThreadRingCount - 1], 0, sizeof(ThreadRingRef));
						--mThreadRingCount;
						break;
					}
				}
			}

			tRegistered = false;
			tOpenCount = 0;
			tClosedHead.store(0, std::memory_order_relaxed);
			tClosedTail.store(0, std::memory_order_relaxed);
			tCurrentTraceId = 0;
		}

		void Tracer::RegisterTraceSink(ITraceSink* sink)
		{
			if (sink == nullptr)
				return;

			std::lock_guard<std::mutex> lock(mRegistryMutex);
			if (mTraceSinkCount >= kMaxTraceSinks)
				return;

			for (unsigned int i = 0; i < mTraceSinkCount; ++i)
			{
				if (mTraceSinks[i] == sink)
					return;
			}

			mTraceSinks[mTraceSinkCount++] = sink;
		}

		void Tracer::UnregisterTraceSink(ITraceSink* sink)
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);
			for (unsigned int i = 0; i < mTraceSinkCount; ++i)
			{
				if (mTraceSinks[i] == sink)
				{
					mTraceSinks[i] = mTraceSinks[mTraceSinkCount - 1];
					mTraceSinks[mTraceSinkCount - 1] = nullptr;
					--mTraceSinkCount;
					return;
				}
			}
		}

		void Tracer::OnSpanOpen(SpanRecord& record)
		{
			if (!tRegistered || !mStarted.load(std::memory_order_acquire))
				return;

			record.spanId = tRng();

			if (tOpenCount == 0)
			{
				// Root span
				record.traceId = tRng();
				tCurrentTraceId = record.traceId;
				record.parentSpanId = 0;
			}
			else
			{
				// Child span
				record.traceId = tCurrentTraceId;
				record.parentSpanId = tOpenStack[tOpenCount - 1].spanId;
			}

			record.scenarioStep = ScenarioStepStack::Current();

			assert(tOpenCount < kOpenStackCapacity);
			tOpenStack[tOpenCount] = record;
			++tOpenCount;
		}

		void Tracer::OnSpanClose(const SpanRecord& record)
		{
			if (!tRegistered || !mStarted.load(std::memory_order_acquire))
				return;

			if (tOpenCount == 0)
				return;

			--tOpenCount;

			if (tOpenCount == 0)
				tCurrentTraceId = 0;

			// Write to closed ring (circular, drops oldest on overflow)
			unsigned int head = tClosedHead.load(std::memory_order_relaxed);
			unsigned int tail = tClosedTail.load(std::memory_order_relaxed);
			tClosedRing[head] = record;
			unsigned int nextHead = (head + 1) % kClosedRingCapacity;

			// If head catches tail, advance tail (drop oldest)
			if (nextHead == tail)
				tClosedTail.store((tail + 1) % kClosedRingCapacity, std::memory_order_release);

			tClosedHead.store(nextHead, std::memory_order_release);
		}

		void Tracer::DrainLoop()
		{
			while (mDrainRunning.load(std::memory_order_acquire))
			{
				DrainAllRings();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		void Tracer::DrainAllRings()
		{
			std::lock_guard<std::mutex> lock(mRegistryMutex);

			for (unsigned int i = 0; i < mThreadRingCount; ++i)
			{
				ThreadRingRef& ref = mThreadRings[i];
				if (ref.closedRing == nullptr)
					continue;

				unsigned int tail = ref.tail->load(std::memory_order_acquire);
				unsigned int head = ref.head->load(std::memory_order_acquire);

				while (tail != head)
				{
					const SpanRecord& span = ref.closedRing[tail];

					if (mSink)
						mSink->OnSpan(span);

					for (unsigned int s = 0; s < mTraceSinkCount; ++s)
					{
						if (mTraceSinks[s])
							mTraceSinks[s]->OnSpan(span);
					}

					tail = (tail + 1) % kClosedRingCapacity;
				}

				ref.tail->store(tail, std::memory_order_release);
			}
		}
	}
} // namespace Observation
} // namespace Dia
