#pragma once

#include <DiaThreading/IJobMetrics.h>
#include <functional>
#include <memory>
#include <cstdint>

namespace Dia
{
	namespace Core { class ThreadPool; }

	namespace Threading
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// JobSystem
		//
		// Task-based parallelism. Each `Submit` enqueues `fn` on a worker thread
		// and returns a refcounted `JobHandle`. The Job is freed when the last
		// handle drops AND the job has finished.
		//
		// USAGE:
		//   JobSystem js;
		//   js.Initialize();                             // 0 = hardware concurrency
		//   JobHandle h = js.Submit([]{ /* work */ });
		//   js.Wait(h);                                  // blocks until h is done
		//   js.Shutdown();                               // joins all workers
		//
		// LIMITATIONS (deliberate):
		//   - No priority scheduling, no parent/child fan-out, no ParallelFor.
		//   - Workers share a single FIFO queue — not work-stealing.
		//   - `Wait` from inside a running job only deadlocks when the pool has
		//     a single worker; with N>=2 workers another thread picks up the job.
		//---------------------------------------------------------------------------------------------------------------------------------

		struct Job;
		using JobFn = std::function<void()>;

		//-----------------------------------------------------------------------------
		// JobHandle — refcounted handle to a submitted job
		//-----------------------------------------------------------------------------
		struct JobHandle
		{
			JobHandle();
			~JobHandle();
			JobHandle(const JobHandle& other);
			JobHandle(JobHandle&& other) noexcept;
			JobHandle& operator=(const JobHandle& other);
			JobHandle& operator=(JobHandle&& other) noexcept;

			bool IsValid() const;

		private:
			friend class JobSystem;
			std::shared_ptr<Job> mPtr;
		};

		//-----------------------------------------------------------------------------
		// JobSystem
		//-----------------------------------------------------------------------------
		class JobSystem
		{
		public:
			JobSystem();
			~JobSystem();

			// numThreads = 0 means use hardware concurrency.
			// metrics may be null — pass a JobSystemMetricsAdapter to enable instrumentation.
			// Calling Initialize twice without an intervening Shutdown asserts.
			void       Initialize(unsigned int numThreads = 0, IJobMetrics* metrics = nullptr);

			// Drains pending tasks then joins all workers. Idempotent.
			void       Shutdown();

			// Returns an invalid handle (and asserts) if called before Initialize
			// or after Shutdown.
			JobHandle  Submit(JobFn fn);

			// Blocks until the job behind h has finished. Safe from any thread.
			void       Wait(const JobHandle& h);

			// True for default-constructed handles or for finished jobs.
			bool       IsComplete(const JobHandle& h) const;

			// --- Metric accessors ---
			size_t   GetQueueDepth()     const;
			int      GetActiveJobCount() const;
			uint64_t GetSubmittedCount() const;
			uint64_t GetCompletedCount() const;
			size_t   GetWorkerCount()    const;

		private:
			Dia::Core::ThreadPool* mThreadPool;
			IJobMetrics*           mMetrics = nullptr;
			[[maybe_unused]] uint64_t mPrevSubmitted = 0;
			[[maybe_unused]] uint64_t mPrevCompleted = 0;
		};

	} // namespace Threading
} // namespace Dia

// Compatibility: old callers used Dia::Core::JobSystem / Dia::Core::JobHandle.
// Forward from the old include path is in DiaCore/Threading/JobSystem.h.
namespace Dia { namespace Core {
	using JobSystem = Dia::Threading::JobSystem;
	using JobHandle = Dia::Threading::JobHandle;
} }
