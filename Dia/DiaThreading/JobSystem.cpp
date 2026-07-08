#include "DiaThreading/JobSystem.h"
#include "DiaCore/Threading/ThreadPool.h"
#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Memory/Memory.h"

#include <atomic>

namespace Dia
{
	namespace Threading
	{
		//-----------------------------------------------------------------------------
		// Job — minimal completion flag, owned by JobHandle's shared_ptr.
		//-----------------------------------------------------------------------------
		struct Job
		{
			std::atomic<int> unfinishedJobs{1}; // 1 until the worker clears it
		};

		//-----------------------------------------------------------------------------
		// JobHandle
		//-----------------------------------------------------------------------------
		JobHandle::JobHandle() : mPtr(nullptr) {}
		JobHandle::~JobHandle() {}
		JobHandle::JobHandle(const JobHandle& other) : mPtr(other.mPtr) {}
		JobHandle::JobHandle(JobHandle&& other) noexcept : mPtr(std::move(other.mPtr)) {}

		JobHandle& JobHandle::operator=(const JobHandle& other)
		{
			if (this != &other) mPtr = other.mPtr;
			return *this;
		}

		JobHandle& JobHandle::operator=(JobHandle&& other) noexcept
		{
			if (this != &other) mPtr = std::move(other.mPtr);
			return *this;
		}

		bool JobHandle::IsValid() const { return mPtr != nullptr; }

		//-----------------------------------------------------------------------------
		// JobSystem
		//-----------------------------------------------------------------------------
		JobSystem::JobSystem() : mThreadPool(nullptr) {}

		JobSystem::~JobSystem() { Shutdown(); }

		void JobSystem::Initialize(unsigned int numThreads, IJobMetrics* metrics)
		{
			DIA_ASSERT(mThreadPool == nullptr, "JobSystem::Initialize called twice without an intervening Shutdown");
			if (mThreadPool)
				return;
			mThreadPool = DIA_NEW(Dia::Core::ThreadPool(numThreads));
			mMetrics    = metrics;
		}

		void JobSystem::Shutdown()
		{
			if (mThreadPool)
			{
				DIA_DELETE(mThreadPool);
				mThreadPool = nullptr;
			}
			mMetrics = nullptr;
		}

		JobHandle JobSystem::Submit(JobFn fn)
		{
			DIA_ASSERT(mThreadPool != nullptr, "JobSystem::Submit called before Initialize or after Shutdown");
			if (!mThreadPool)
				return JobHandle();

			auto jobPtr = std::make_shared<Job>();

			// Capture metrics and pool by value so the lambda is self-contained.
			auto* metrics = mMetrics;
			mThreadPool->Enqueue([fn = std::move(fn), jobPtr,
			                      metrics,
			                      pool = mThreadPool]() {
				if (fn) fn();
				jobPtr->unfinishedJobs.store(0, std::memory_order_release);
				if (metrics)
				{
					metrics->OnJobCompleted();
					metrics->OnActiveWorkersChanged(static_cast<double>(pool->GetActiveTaskCount()));
					metrics->OnQueueDepthChanged   (static_cast<double>(pool->GetQueueDepth()));
				}
			});

			if (mMetrics)
			{
				mMetrics->OnJobSubmitted();
				mMetrics->OnQueueDepthChanged(static_cast<double>(mThreadPool->GetQueueDepth()));
			}

			JobHandle handle;
			handle.mPtr = jobPtr;
			return handle;
		}

		void JobSystem::Wait(const JobHandle& h)
		{
			if (!h.IsValid()) return;
			while (h.mPtr->unfinishedJobs.load(std::memory_order_acquire) > 0)
				Dia::Core::ThisThread::Yield();
		}

		bool JobSystem::IsComplete(const JobHandle& h) const
		{
			if (!h.IsValid()) return true;
			return h.mPtr->unfinishedJobs.load(std::memory_order_acquire) == 0;
		}

		size_t   JobSystem::GetQueueDepth()     const { return mThreadPool ? mThreadPool->GetQueueDepth()     : 0; }
		int      JobSystem::GetActiveJobCount() const { return mThreadPool ? mThreadPool->GetActiveTaskCount() : 0; }
		uint64_t JobSystem::GetSubmittedCount() const { return mThreadPool ? mThreadPool->GetSubmittedCount()  : 0; }
		uint64_t JobSystem::GetCompletedCount() const { return mThreadPool ? mThreadPool->GetCompletedCount()  : 0; }
		size_t   JobSystem::GetWorkerCount()    const { return mThreadPool ? mThreadPool->GetThreadCount()     : 0; }

	} // namespace Threading
} // namespace Dia
