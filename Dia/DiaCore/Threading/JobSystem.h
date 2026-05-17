#ifndef DIA_JOB_SYSTEM_H
#define DIA_JOB_SYSTEM_H

#include "DiaCore/Threading/ThreadPool.h"
#include "DiaCore/Threading/Atomic.h"
#include "DiaCore/Memory/Memory.h"
#include <functional>
#include <memory>

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
		// NEW INSTANCE API (Phase 1):
		//   JobSystem js;
		//   js.Init();
		//   JobHandle h = js.Submit([]() { /* work */ });
		//   js.Wait(h);
		//   js.Quit();
		//
		// LEGACY STATIC API (preserved for backward compatibility):
		//   JobSystem::Initialize();
		//   Job* job = JobSystem::CreateJob([](Job*) { /* work */ });
		//   JobSystem::Run(job);
		//   JobSystem::Wait(job);
		//   JobSystem::Shutdown();
		//---------------------------------------------------------------------------------------------------------------------------------

		// Forward declaration - internal Job struct used by legacy API
		struct Job;

		// New instance API types
		using JobFn = std::function<void()>;

		//-----------------------------------------------------------------------------
		// JobHandle - refcounted handle to a job
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
		// Legacy Job struct (used by static shim API)
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
		// JobSystem - task-based parallelism
		//-----------------------------------------------------------------------------
		class JobSystem
		{
		public:
			JobSystem();
			~JobSystem();

			// Instance API (Phase 1 - temporary names to avoid collision with static shim)
			void Init(unsigned int numThreads = 0);
			void Quit();
			JobHandle Submit(JobFn fn);
			void Wait(const JobHandle& h);
			bool IsComplete(const JobHandle& h) const;

			// Legacy static shim API (preserved for backward compatibility)
			static void Initialize(unsigned int numThreads = 0);
			static void Shutdown();
			static Job* CreateJob(Job::JobFunc function);
			static Job* CreateChildJob(Job* parent, Job::JobFunc function);
			static void Run(Job* job);
			static void Wait(Job* job);
			static bool IsComplete(Job* job);

		private:
			// Legacy implementation methods (called by static shim)
			void InitializeImpl(unsigned int numThreads);
			void ShutdownImpl();
			Job* CreateJobImpl(Job::JobFunc function);
			Job* CreateChildJobImpl(Job* parent, Job::JobFunc function);
			void RunImpl(Job* job);
			void WaitImpl(Job* job);

			// Helper for static shim
			static JobSystem& GetInstance();
			static void Execute(Job* job);
			static void Finish(Job* job);

			ThreadPool* mThreadPool;
		};

		//-----------------------------------------------------------------------------
		// Parallel for helper (legacy API)
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
