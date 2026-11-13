#pragma once

#include <DiaThreading/IJobObserver.h>
#include <DiaObservation/Job/JobInbox.h>

#include <atomic>
#include <cstdint>

namespace Dia
{
	namespace Observation
	{
		namespace Job
		{
			// IJobObserver implementation: registers worker threads with the log/trace/profile
			// pillars, captures the submitter's attribution, wraps each job in a `job.run`
			// profile scope carrying the owner as subject, and delivers the timing record to
			// the submitting PU's inbox.
			//
			// A PU installs its inbox for its own thread at start:
			//     JobObserver::InstallThreadInbox(&myInbox);   // ... and ClearThreadInbox() at stop
			// Threads without an inbox submit with inbox == nullptr; their records are only counted.
			//
			// Worker thread naming ("Worker N", B1) and the `profile_job` record (B2, pushed to the
			// worker's job ring, written by the profile drain) are done.
			// traceParent is always 0 until Tracer exposes the active span id.
			class JobObserver : public Dia::Threading::IJobObserver
			{
			public:
				static void InstallThreadInbox(JobInbox* inbox);
				static void ClearThreadInbox();

				void OnWorkerStart(unsigned workerIndex) override;
				void OnWorkerStop(unsigned workerIndex) override;
				Dia::Threading::JobContext CaptureContext() override;
				void OnJobBegin(const Dia::Threading::JobContext& ctx) override;
				void OnJobEnd(const Dia::Threading::JobContext& ctx,
				              const Dia::Threading::JobTimingRecord& rec) override;

				// Records finished with no inbox (counted, not kept).
				uint64_t UnownedCount() const { return mUnowned.load(std::memory_order_relaxed); }

			private:
				std::atomic<uint64_t> mUnowned{0};
			};
		}
	}
}
