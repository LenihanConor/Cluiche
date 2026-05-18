#ifndef DIA_JOB_SYSTEM_H
#define DIA_JOB_SYSTEM_H

#include <functional>
#include <memory>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// JobSystem
		//
		// Task-based parallelism. Each `Submit` enqueues `fn` on a worker thread
		// and returns a refcounted `JobHandle`. The Job is freed when the last
		// handle drops AND the job has finished — no manual delete, no double-Run
		// hazard, no UAF on `IsComplete` after `Wait`.
		//
		// USAGE:
		//   JobSystem js;
		//   js.Initialize();                            // 0 = hardware concurrency
		//   JobHandle h = js.Submit([]{ /* work */ });
		//   js.Wait(h);                                 // blocks until h is done
		//   js.Shutdown();                              // joins all workers
		//
		// LIMITATIONS (deliberate):
		//   - No priority scheduling, no parent/child fan-out, no ParallelFor.
		//     Reintroduce via separate spec when a real caller needs them.
		//   - Workers share a single FIFO queue — not work-stealing.
		//   - `Wait` from inside a running job only deadlocks when the pool has
		//     a single worker; with N>=2 workers another thread picks up the
		//     waited-on job.
		//---------------------------------------------------------------------------------------------------------------------------------

		class ThreadPool;

		// Internal job state — defined in the .cpp.
		struct Job;

		using JobFn = std::function<void()>;

		//-----------------------------------------------------------------------------
		// JobHandle - refcounted handle to a submitted job
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
			// Calling Initialize twice without an intervening Shutdown asserts.
			void       Initialize(unsigned int numThreads = 0);

			// Drains pending tasks then joins all workers. Idempotent.
			void       Shutdown();

			// Returns an invalid handle (and asserts) if called before Initialize
			// or after Shutdown.
			JobHandle  Submit(JobFn fn);

			// Blocks until the job behind h has finished. Safe to call from any
			// thread; safe on a default-constructed handle (returns immediately).
			void       Wait(const JobHandle& h);

			// True for default-constructed handles or for finished jobs.
			bool       IsComplete(const JobHandle& h) const;

		private:
			ThreadPool* mThreadPool;
		};
	}
}

#endif // DIA_JOB_SYSTEM_H
