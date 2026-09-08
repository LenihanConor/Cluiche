#include "DiaCore/Threading/JobSystem.h"
#include "DiaCore/Threading/ThreadPool.h"
#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Memory/Memory.h"

#include <atomic>

namespace Dia
{
	namespace Core
	{
		//-----------------------------------------------------------------------------
		// Job - minimal completion flag, owned by JobHandle's shared_ptr.
		//-----------------------------------------------------------------------------
		struct Job
		{
			std::atomic<int> unfinishedJobs{1}; // 1 until the worker clears it
		};

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
		// JobSystem
		//-----------------------------------------------------------------------------
		JobSystem::JobSystem()
			: mThreadPool(nullptr)
		{}

		JobSystem::~JobSystem()
		{
			Shutdown();
		}

		void JobSystem::Initialize(unsigned int numThreads)
		{
			DIA_ASSERT(mThreadPool == nullptr, "JobSystem::Initialize called twice without an intervening Shutdown");
			if (mThreadPool)
				return;

			mThreadPool = DIA_NEW(ThreadPool(numThreads));
		}

		void JobSystem::Shutdown()
		{
			if (mThreadPool)
			{
				// ThreadPool::~ThreadPool drains + joins (see ThreadPool.cpp).
				DIA_DELETE(mThreadPool);
				mThreadPool = nullptr;
			}
		}

		JobHandle JobSystem::Submit(JobFn fn)
		{
			DIA_ASSERT(mThreadPool != nullptr, "JobSystem::Submit called before Initialize or after Shutdown");
			if (!mThreadPool)
				return JobHandle();

			auto jobPtr = std::make_shared<Job>();

			// The enqueued task owns fn and a reference to jobPtr; the worker drops
			// both when the task lambda goes out of scope after execution. Storing fn
			// inside jobPtr would create a Job->function->shared_ptr<Job> cycle that
			// prevents the Job from ever being freed.
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

			while (h.mPtr->unfinishedJobs.load(std::memory_order_acquire) > 0)
			{
				ThisThread::Yield();
			}
		}

		bool JobSystem::IsComplete(const JobHandle& h) const
		{
			if (!h.IsValid())
				return true;

			return h.mPtr->unfinishedJobs.load(std::memory_order_acquire) == 0;
		}
	}
}
