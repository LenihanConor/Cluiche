#pragma once

#include <DiaObservation/Trace/SpanRecord.h>

#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		class TraceFileSink;
		class ITraceSink;

		class Tracer
		{
		public:
			static Tracer& Instance();

			bool Start(const char* traceFilePath, TraceCategory activeMask,
			           const char* sessionId, int64_t epochOffsetNs);
			void Stop();

			void RegisterThreadSpanBuffer();
			void UnregisterThreadSpanBuffer();

			void            SetActiveMask(TraceCategory mask);
			TraceCategory   ActiveMask() const;

			void OnSpanOpen(SpanRecord& record);
			void OnSpanClose(const SpanRecord& record);

			void RegisterTraceSink(ITraceSink* sink);
			void UnregisterTraceSink(ITraceSink* sink);

			bool IsStarted() const { return mStarted.load(std::memory_order_acquire); }

		private:
			Tracer();
			~Tracer();

			Tracer(const Tracer&) = delete;
			Tracer& operator=(const Tracer&) = delete;

			void DrainLoop();
			void DrainAllRings();

			struct ThreadRingRef
			{
				SpanRecord*                closedRing;
				std::atomic<unsigned int>* head;
				std::atomic<unsigned int>* tail;
			};

			static const unsigned int kMaxThreadRings = 16;

			ThreadRingRef    mThreadRings[kMaxThreadRings];
			unsigned int     mThreadRingCount;
			std::mutex       mRegistryMutex;

			TraceFileSink*              mSink;
			uint64_t                    mSessionSeed;
			std::atomic<TraceCategory>  mActiveMask;

			static const unsigned int kMaxTraceSinks = 4;
			ITraceSink*      mTraceSinks[kMaxTraceSinks];
			unsigned int     mTraceSinkCount;

			std::thread      mDrainThread;
			std::atomic<bool> mDrainRunning;
			std::atomic<bool> mStarted;
		};
	}
} // namespace Observation
} // namespace Dia
