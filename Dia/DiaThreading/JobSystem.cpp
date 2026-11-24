#include "DiaThreading/JobSystem.h"
#include "DiaCore/Threading/ThreadPool.h"
#include "DiaCore/Threading/Thread.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Memory/Memory.h"

#include <atomic>
#include <chrono>

namespace Dia
{
	namespace Threading
	{
		namespace
		{
			thread_local uint16_t tWorkerIndex = 0xFFFF;

			uint64_t NowNs()
			{
				return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
					std::chrono::steady_clock::now().time_since_epoch()).count());
			}
		}

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

		void JobSystem::Initialize(unsigned int numThreads, IJobMetrics* metrics, IJobObserver* observer)
		{
			DIA_ASSERT(mThreadPool == nullptr, "JobSystem::Initialize called twice without an intervening Shutdown");
			if (mThreadPool)
				return;
			if (observer)
			{
				Dia::Core::ThreadPool::ThreadHooks hooks;
				hooks.onStart = [observer](unsigned int i) {
					tWorkerIndex = static_cast<uint16_t>(i);
					observer->OnWorkerStart(i);
				};
				hooks.onStop = [observer](unsigned int i) {
					observer->OnWorkerStop(i);
				};
				mThreadPool = DIA_NEW(Dia::Core::ThreadPool(numThreads, std::move(hooks)));
			}
			else
			{
				mThreadPool = DIA_NEW(Dia::Core::ThreadPool(numThreads));
			}
			mMetrics  = metrics;
			mObserver = observer;
		}

		void JobSystem::Shutdown()
		{
			if (mThreadPool)
			{
				DIA_DELETE(mThreadPool);
				mThreadPool = nullptr;
			}
			mMetrics  = nullptr;
			mObserver = nullptr;
		}

		JobHandle JobSystem::Submit(JobFn fn)
		{
			DIA_ASSERT(mThreadPool != nullptr, "JobSystem::Submit called before Initialize or after Shutdown");
			if (!mThreadPool)
				return JobHandle();
			return SubmitImpl(std::move(fn), nullptr);
		}

		JobHandle JobSystem::Submit(JobFn fn, const JobTag& tag)
		{
			DIA_ASSERT(mThreadPool != nullptr, "JobSystem::Submit called before Initialize or after Shutdown");
			if (!mThreadPool)
				return JobHandle();
			return SubmitImpl(std::move(fn), &tag);
		}

		JobHandle JobSystem::TrySubmit(JobFn fn)
		{
			// Non-asserting variant: return invalid handle if pool is null.
			if (!mThreadPool)
				return JobHandle();
			return SubmitImpl(std::move(fn), nullptr);
		}

		JobHandle JobSystem::TrySubmit(JobFn fn, const JobTag& tag)
		{
			// Non-asserting tagged variant: invalid handle if pool is null.
			if (!mThreadPool)
				return JobHandle();
			return SubmitImpl(std::move(fn), &tag);
		}

		JobHandle JobSystem::SubmitImpl(JobFn fn, const JobTag* tag)
		{
			auto jobPtr = std::make_shared<Job>();

			// Capture metrics and pool by value so the lambda is self-contained.
			auto* metrics  = mMetrics;
			auto* observer = mObserver;

			if (!observer)
			{
				mThreadPool->Enqueue([fn = std::move(fn), jobPtr,
				                      metrics,
				                      pool = mThreadPool]() {
					// Runs on all exits (including exceptions) so Wait() never hangs.
					struct Complete
					{
						Job* job; IJobMetrics* metrics; Dia::Core::ThreadPool* pool;
						~Complete()
						{
							job->unfinishedJobs.store(0, std::memory_order_release);
							if (metrics)
							{
								metrics->OnJobCompleted();
								metrics->OnActiveWorkersChanged(static_cast<double>(pool->GetActiveTaskCount()));
								metrics->OnQueueDepthChanged   (static_cast<double>(pool->GetQueueDepth()));
							}
						}
					} complete{jobPtr.get(), metrics, pool};
					if (fn) fn();
				});
			}
			else
			{
				JobContext ctx = observer->CaptureContext();
				if (tag)
					ctx.attribution = *tag;
				const uint64_t submitNs = NowNs();

				mThreadPool->Enqueue([fn = std::move(fn), jobPtr,
				                      metrics, observer, ctx, submitNs,
				                      pool = mThreadPool]() {
					const uint64_t startNs = NowNs();
					// Runs on all exits (including exceptions): end notification, then completion.
					struct Finish
					{
						IJobObserver* observer; const JobContext& ctx; uint64_t submitNs; uint64_t startNs;
						Job* job; IJobMetrics* metrics; Dia::Core::ThreadPool* pool;
						~Finish()
						{
							JobTimingRecord rec{};
							rec.submitNs   = submitNs;
							rec.startNs    = startNs;
							rec.endNs      = NowNs();
							rec.owner      = ctx.attribution.owner;
							rec.tag        = ctx.attribution.tag;
							rec.range      = ctx.attribution.range;
							rec.rangeCount = ctx.attribution.rangeCount;
							rec.worker     = tWorkerIndex;
							rec.flags      = 0;
							observer->OnJobEnd(ctx, rec);
							job->unfinishedJobs.store(0, std::memory_order_release);
							if (metrics)
							{
								metrics->OnJobCompleted();
								metrics->OnActiveWorkersChanged(static_cast<double>(pool->GetActiveTaskCount()));
								metrics->OnQueueDepthChanged   (static_cast<double>(pool->GetQueueDepth()));
							}
						}
					} finish{observer, ctx, submitNs, startNs, jobPtr.get(), metrics, pool};

					observer->OnJobBegin(ctx);
					if (fn) fn();
				});
			}

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
