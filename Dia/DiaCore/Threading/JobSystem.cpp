#include "DiaCore/Threading/JobSystem.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------------
		// JobHandle implementation
		//-----------------------------------------------------------------------------
		JobHandle::JobHandle()
			: mPtr(nullptr)
		{}

		JobHandle::~JobHandle()
		{}

		JobHandle::JobHandle(const JobHandle& other)
			: mPtr(other.mPtr)
		{}

		JobHandle::JobHandle(JobHandle&& other) noexcept
			: mPtr(std::move(other.mPtr))
		{}

		JobHandle& JobHandle::operator=(const JobHandle& other)
		{
			if (this != &other)
			{
				mPtr = other.mPtr;
			}
			return *this;
		}

		JobHandle& JobHandle::operator=(JobHandle&& other) noexcept
		{
			if (this != &other)
			{
				mPtr = std::move(other.mPtr);
			}
			return *this;
		}

		bool JobHandle::IsValid() const
		{
			return mPtr != nullptr;
		}

		//-----------------------------------------------------------------------------
		// JobSystem instance API implementation
		//-----------------------------------------------------------------------------
		JobSystem::JobSystem()
			: mThreadPool(nullptr)
		{}

		JobSystem::~JobSystem()
		{
			Quit();
		}

		void JobSystem::Init(unsigned int numThreads)
		{
			if (!mThreadPool)
			{
				mThreadPool = DIA_NEW(ThreadPool(numThreads));
			}
			// Double-Init is a no-op (preserves existing behaviour)
		}

		void JobSystem::Quit()
		{
			if (mThreadPool)
			{
				DIA_DELETE(mThreadPool);
				mThreadPool = nullptr;
			}
		}

		JobHandle JobSystem::Submit(JobFn fn)
		{
			DIA_ASSERT(mThreadPool != nullptr, "JobSystem::Submit called before Init or after Quit");
			if (!mThreadPool)
				return JobHandle();

			auto jobPtr = std::make_shared<Job>();

			// The enqueued task owns fn and a reference to jobPtr; the worker drops
			// both when the task lambda goes out of scope after execution. Storing fn
			// inside jobPtr->function would create a Job->function->shared_ptr<Job>
			// cycle that prevents the Job from ever being freed.
			mThreadPool->Enqueue([fn = std::move(fn), jobPtr]() {
				if (fn)
				{
					fn();
				}
				jobPtr->unfinishedJobs.store(0, std::memory_order_release);
			});

			JobHandle handle;
			handle.mPtr = jobPtr;
			return handle;
		}

		void JobSystem::Wait(const JobHandle& h)
		{
			if (!h.IsValid())
				return;

			// Spin-yield while the job is incomplete
			while (h.mPtr->unfinishedJobs.load(std::memory_order_acquire) > 0)
			{
				ThisThread::Yield();
			}
			// The shared_ptr keeps the Job alive - no manual delete needed
		}

		bool JobSystem::IsComplete(const JobHandle& h) const
		{
			if (!h.IsValid())
				return true;

			return h.mPtr->unfinishedJobs.load(std::memory_order_acquire) == 0;
		}

		//-----------------------------------------------------------------------------
		// Static shim implementation (delegates to global Meyers singleton)
		//-----------------------------------------------------------------------------
		JobSystem& JobSystem::GetInstance()
		{
			static JobSystem instance;
			return instance;
		}

		void JobSystem::Initialize(unsigned int numThreads)
		{
			GetInstance().InitializeImpl(numThreads);
		}

		void JobSystem::Shutdown()
		{
			GetInstance().ShutdownImpl();
		}

		Job* JobSystem::CreateJob(Job::JobFunc function)
		{
			return GetInstance().CreateJobImpl(function);
		}

		Job* JobSystem::CreateChildJob(Job* parent, Job::JobFunc function)
		{
			return GetInstance().CreateChildJobImpl(parent, function);
		}

		void JobSystem::Run(Job* job)
		{
			GetInstance().RunImpl(job);
		}

		void JobSystem::Wait(Job* job)
		{
			GetInstance().WaitImpl(job);
		}

		bool JobSystem::IsComplete(Job* job)
		{
			return job->unfinishedJobs.load(std::memory_order_acquire) == 0;
		}

		//-----------------------------------------------------------------------------
		// Legacy implementation methods (preserve exact existing behaviour)
		//-----------------------------------------------------------------------------
		void JobSystem::InitializeImpl(unsigned int numThreads)
		{
			if (!mThreadPool)
			{
				mThreadPool = DIA_NEW(ThreadPool(numThreads));
			}
		}

		void JobSystem::ShutdownImpl()
		{
			if (mThreadPool)
			{
				DIA_DELETE(mThreadPool);
				mThreadPool = nullptr;
			}
		}

		Job* JobSystem::CreateJobImpl(Job::JobFunc function)
		{
			Job* job = DIA_NEW(Job());
			job->function = function;
			job->parent = nullptr;
			return job;
		}

		Job* JobSystem::CreateChildJobImpl(Job* parent, Job::JobFunc function)
		{
			// Increment parent's unfinished count
			parent->unfinishedJobs.fetch_add(1, std::memory_order_relaxed);

			Job* job = DIA_NEW(Job());
			job->function = function;
			job->parent = parent;
			return job;
		}

		void JobSystem::RunImpl(Job* job)
		{
			if (!mThreadPool) return;

			mThreadPool->Enqueue([job]() {
				Execute(job);
			});
		}

		void JobSystem::WaitImpl(Job* job)
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

		void JobSystem::Execute(Job* job)
		{
			// Execute the job function
			if (job->function)
			{
				job->function(job);
			}

			// Finish the job
			Finish(job);
		}

		void JobSystem::Finish(Job* job)
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
	}
}
