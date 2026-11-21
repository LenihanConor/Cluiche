#pragma once

#include <DiaThreading/JobContext.h>

namespace Dia
{
	namespace Threading
	{
		// Observer hooks for JobSystem. Implementations live outside DiaThreading.
		class IJobObserver
		{
		public:
			virtual ~IJobObserver() = default;
			virtual void       OnWorkerStart(unsigned workerIndex) = 0;   // worker thread
			virtual void       OnWorkerStop (unsigned workerIndex) = 0;   // worker thread
			virtual JobContext CaptureContext() = 0;                       // submitting thread
			virtual void       OnJobBegin(const JobContext& ctx) = 0;      // worker thread, before fn
			virtual void       OnJobEnd  (const JobContext& ctx,
			                              const JobTimingRecord& rec) = 0; // worker thread, after fn
		};
	}
}
