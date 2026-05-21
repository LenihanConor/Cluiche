#include "DiaThreading/JobSystem.h"
#include "DiaCore/Threading/ThreadPool.h"
#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Memory/Memory.h"
#include "DiaObservation/Metric/MetricRegistry.h"
#include "DiaObservation/Metric/Gauge.h"
#include "DiaObservation/Metric/Counter.h"

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

		void JobSystem::Initialize(unsigned int numThreads)
		{
			DIA_ASSERT(mThreadPool == nullptr, "JobSystem::Initialize called twice without an intervening Shutdown");
			if (mThreadPool)
				return;
			mThreadPool = DIA_NEW(Dia::Core::ThreadPool(numThreads));

			auto& reg            = Dia::Observation::Metric::MetricRegistry::Instance();
			mMetricQueueDepth    = reg.RegisterGauge  (Dia::Core::StringCRC("dia.jobs.queue_depth"));
			mMetricActiveWorkers = reg.RegisterGauge  (Dia::Core::StringCRC("dia.jobs.active_workers"));
			mMetricSubmitted     = reg.RegisterCounter(Dia::Core::StringCRC("dia.jobs.submitted"));
			mMetricCompleted     = reg.RegisterCounter(Dia::Core::StringCRC("dia.jobs.completed"));
		}

		void JobSystem::Shutdown()
		{
			if (mThreadPool)
			{
				DIA_DELETE(mThreadPool);
				mThreadPool = nullptr;
			}
			mMetricQueueDepth    = nullptr;
			mMetricActiveWorkers = nullptr;
			mMetricSubmitted     = nullptr;
			mMetricCompleted     = nullptr;
		}

		JobHandle JobSystem::Submit(JobFn fn)
		{
			DIA_ASSERT(mThreadPool != nullptr, "JobSystem::Submit called before Initialize or after Shutdown");
			if (!mThreadPool)
				return JobHandle();

			auto jobPtr = std::make_shared<Job>();

			// Capture metric pointers by value so the lambda is self-contained.
			auto* metricCompleted     = mMetricCompleted;
			auto* metricActiveWorkers = mMetricActiveWorkers;
			auto* metricQueueDepth    = mMetricQueueDepth;
			mThreadPool->Enqueue([fn = std::move(fn), jobPtr,
			                      metricCompleted, metricActiveWorkers, metricQueueDepth,
			                      pool = mThreadPool]() {
				if (fn) fn();
				jobPtr->unfinishedJobs.store(0, std::memory_order_release);
				if (metricCompleted)     metricCompleted->Inc();
				if (metricActiveWorkers) metricActiveWorkers->Set(static_cast<double>(pool->GetActiveTaskCount()));
				if (metricQueueDepth)    metricQueueDepth->Set   (static_cast<double>(pool->GetQueueDepth()));
			});

			if (mMetricSubmitted)  mMetricSubmitted->Inc();
			if (mMetricQueueDepth) mMetricQueueDepth->Set(static_cast<double>(mThreadPool->GetQueueDepth()));

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
