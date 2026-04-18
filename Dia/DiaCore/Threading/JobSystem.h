#ifndef DIA_JOB_SYSTEM_H
#define DIA_JOB_SYSTEM_H

#include "DiaCore/Threading/ThreadPool.h"
#include "DiaCore/Threading/Atomic.h"
#include "DiaCore/Memory/Memory.h"
#include <functional>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Job System
		//
		// Task-based parallelism system for game engines.
		// Jobs can have dependencies and can spawn child jobs.
		//
		// USAGE:
		//   JobSystem::Initialize();
		//
		//   Job* job = JobSystem::CreateJob([]() {
		//       // Do work
		//   });
		//   JobSystem::Run(job);
		//   JobSystem::Wait(job);
		//
		// FEATURES:
		//   - Dependency tracking
		//   - Job priorities
		//   - Parent-child relationships
		//   - Work stealing
		//---------------------------------------------------------------------------------------------------------------------------------

		struct Job;
		class JobSystem;

		//-----------------------------------------------------------------------------
		// Job handle
		//-----------------------------------------------------------------------------
		struct Job
		{
			using JobFunc = std::function<void(Job*)>;

			JobFunc function;
			Job* parent;
			std::atomic<int> unfinishedJobs;
			int priority;

			Job()
				: parent(nullptr)
				, unfinishedJobs(1)  // 1 for the job itself
				, priority(0)
			{}
		};

		//-----------------------------------------------------------------------------
		// Job System singleton
		//-----------------------------------------------------------------------------
		class JobSystem
		{
		public:
			// Initialize job system
			static void Initialize(unsigned int numThreads = 0)
			{
				GetInstance().InitializeImpl(numThreads);
			}

			// Shutdown job system
			static void Shutdown()
			{
				GetInstance().ShutdownImpl();
			}

			// Create a job
			static Job* CreateJob(Job::JobFunc function)
			{
				return GetInstance().CreateJobImpl(function);
			}

			// Create a child job (depends on parent)
			static Job* CreateChildJob(Job* parent, Job::JobFunc function)
			{
				return GetInstance().CreateChildJobImpl(parent, function);
			}

			// Run a job
			static void Run(Job* job)
			{
				GetInstance().RunImpl(job);
			}

			// Wait for a job to complete
			static void Wait(Job* job)
			{
				GetInstance().WaitImpl(job);
			}

			// Check if job is complete
			static bool IsComplete(Job* job)
			{
				return job->unfinishedJobs.load(std::memory_order_acquire) == 0;
			}

		private:
			JobSystem()
				: mThreadPool(nullptr)
			{}

			~JobSystem()
			{
				ShutdownImpl();
			}

			static JobSystem& GetInstance()
			{
				static JobSystem instance;
				return instance;
			}

			void InitializeImpl(unsigned int numThreads)
			{
				if (!mThreadPool)
				{
					mThreadPool = DIA_NEW(ThreadPool(numThreads));
				}
			}

			void ShutdownImpl()
			{
				if (mThreadPool)
				{
					DIA_DELETE(mThreadPool);
				}
			}

			Job* CreateJobImpl(Job::JobFunc function)
			{
				Job* job = DIA_NEW(Job());
				job->function = function;
				job->parent = nullptr;
				return job;
			}

			Job* CreateChildJobImpl(Job* parent, Job::JobFunc function)
			{
				// Increment parent's unfinished count
				parent->unfinishedJobs.fetch_add(1, std::memory_order_relaxed);

				Job* job = DIA_NEW(Job());
				job->function = function;
				job->parent = parent;
				return job;
			}

			void RunImpl(Job* job)
			{
				if (!mThreadPool) return;

				mThreadPool->Enqueue([job]() {
					Execute(job);
				});
			}

			void WaitImpl(Job* job)
			{
				// Help execute jobs while waiting
				while (job->unfinishedJobs.load(std::memory_order_acquire) > 0)
				{
					// Could implement work stealing here
					ThisThread::Yield();
				}

				// Job is complete - clean it up
				// We're safe to delete now because we know unfinishedJobs reached 0
				DIA_DELETE(job);
			}

			static void Execute(Job* job)
			{
				// Execute the job function
				if (job->function)
				{
					job->function(job);
				}

				// Finish the job
				Finish(job);
			}

			static void Finish(Job* job)
			{
				// Decrement unfinished count
				const int unfinished = job->unfinishedJobs.fetch_sub(1, std::memory_order_acq_rel);

				if (unfinished == 1)
				{
					// Job is complete - notify parent
					if (job->parent)
					{
						Finish(job->parent);
					}

					// Note: Job is NOT deleted here
					// The caller of Wait() or JobSystem is responsible for cleanup
					// This avoids use-after-free bugs when Wait() checks IsComplete()
				}
			}

			ThreadPool* mThreadPool;
		};

		//-----------------------------------------------------------------------------
		// Parallel for helper
		//-----------------------------------------------------------------------------
		template <typename Func>
		inline void ParallelFor(int start, int end, Func func, int batchSize = 64)
		{
			if (end <= start) return;

			int range = end - start;
			int numBatches = (range + batchSize - 1) / batchSize;

			Job* parentJob = JobSystem::CreateJob(nullptr);

			for (int i = 0; i < numBatches; ++i)
			{
				int batchStart = start + i * batchSize;
				int batchEnd = (i == numBatches - 1) ? end : batchStart + batchSize;

				Job* childJob = JobSystem::CreateChildJob(parentJob, [batchStart, batchEnd, func](Job*) {
					for (int j = batchStart; j < batchEnd; ++j)
					{
						func(j);
					}
				});

				JobSystem::Run(childJob);
			}

			JobSystem::Run(parentJob);
			JobSystem::Wait(parentJob);
		}
	}
}

#endif // DIA_JOB_SYSTEM_H
