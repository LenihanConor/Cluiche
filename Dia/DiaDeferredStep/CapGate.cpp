#include <DiaDeferredStep/CapGate.h>

#include <chrono>
#include <DiaCore/Core/Assert.h>
#include <DiaDeferredStep/DeferredStepObservation.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaThreading/JobSystem.h>

namespace Dia::DeferredStep
{
	namespace
	{
		uint64_t NowNs()
		{
			return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
		}

		template<typename T>
		void RaiseTo(std::atomic<T>& peak, T value)
		{
			T seen = peak.load();
			while (value > seen && !peak.compare_exchange_weak(seen, value)) {}
		}
	}

	CapGate::CapGate(Dia::Threading::JobSystem& jobs, const CapConfig& config)
		: mJobs(jobs)
	{
		bool clamped = false;
		const size_t workers = jobs.GetWorkerCount();
		mWorkerCap = ResolveWorkerCap(config, workers, &clamped);
		if (clamped)
		{
			DIA_LOG_WARNING(DeferredStepObservation::kLogChannel,
				"Worker cap %u exceeds worker count %zu; clamped to %u", config.workerCap, workers, mWorkerCap);
		}
		const uint32_t perWorker = config.rangesPerWorker == 0 ? 1u : config.rangesPerWorker;
		mRangesPerWorker = perWorker;
		mQueue.assign(static_cast<size_t>(kMaxChannelsHint) * (static_cast<size_t>(mWorkerCap) * perWorker + 1), nullptr);
	}

	CapGate::~CapGate()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		DIA_ASSERT(mSlotsHeldLocked == 0, "CapGate destroyed with slots still held");
		DIA_ASSERT(mCount == 0, "CapGate destroyed with items still queued");
	}

	CapStats CapGate::GetStats() const
	{
		CapStats s;
		s.slotsHeld         = mSlotsHeld.load();
		s.peakSlotsHeld     = mPeakSlotsHeld.load();
		s.workerCap         = mWorkerCap;
		s.admitted          = mAdmitted.load();
		s.queuedAtLeastOnce = mQueuedAtLeastOnce.load();
		s.stolen            = mStolen.load();
		s.trySubmitFailed   = mTrySubmitFailed.load();
		s.totalCapWaitNs    = mTotalCapWaitNs.load();
		s.peakQueueDepth    = mPeakQueueDepth.load();
		return s;
	}

	void CapGate::ResetStats()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mPeakSlotsHeld.store(mSlotsHeldLocked);
		mAdmitted.store(0);
		mQueuedAtLeastOnce.store(0);
		mStolen.store(0);
		mTrySubmitFailed.store(0);
		mTotalCapWaitNs.store(0);
		mPeakQueueDepth.store(mCount);
	}

	bool CapGate::SubmitLocked(GateItem& item)
	{
		// Caller holds mMutex and has verified mSlotsHeldLocked < mWorkerCap.
		++mSlotsHeldLocked;
		item.mState.store(GateItem::State::kRunning);

		Dia::Threading::JobHandle handle;
		const bool forceFail = mFailureHook && mFailureHook();
		if (!forceFail)
		{
			GateItem* itemPtr = &item;
			handle = mJobs.TrySubmit([this, itemPtr]
			{
				// Release the slot even if the work throws (JobSystem records the exception).
				struct Epilogue
				{
					CapGate& gate;
					GateItem& item;
					~Epilogue() { gate.Finish(item); }
				} epilogue{ *this, *itemPtr };
				itemPtr->mWork();
			});
		}

		if (!handle.IsValid())
		{
			--mSlotsHeldLocked;
			mTrySubmitFailed.fetch_add(1);
			item.mState.store(GateItem::State::kSubmitFailed);
			mDoneCv.notify_all();
			return false;
		}

		mSlotsHeld.store(mSlotsHeldLocked);
		mAdmitted.fetch_add(1);
		RaiseTo(mPeakSlotsHeld, mSlotsHeldLocked);
		return true;
	}

	void CapGate::PumpLocked()
	{
		while (mSlotsHeldLocked < mWorkerCap && mCount > 0)
		{
			GateItem* next = mQueue[mHead];
			mQueue[mHead] = nullptr;
			mHead = (mHead + 1) % mQueue.size();
			--mCount;

			next->mAdmitNs = NowNs();
			const uint64_t waited = next->mAdmitNs - next->mEnqueueNs;
			if (SubmitLocked(*next))
			{
				mTotalCapWaitNs.fetch_add(waited);
			}
			// On failure SubmitLocked already marked kSubmitFailed and woke the waiter.
		}
	}

	void CapGate::EnqueueLocked(GateItem* item)
	{
		mQueue[(mHead + mCount) % mQueue.size()] = item;
		++mCount;
		RaiseTo<uint64_t>(mPeakQueueDepth, mCount);
	}

	void CapGate::Finish(GateItem& item)
	{
		// Last touch of `item` by the worker: once state is kDone and the lock is released the owner
		// may destroy it. Notify while still holding the lock so the gate cannot be destroyed under us.
		std::lock_guard<std::mutex> lock(mMutex);
		--mSlotsHeldLocked;
		mSlotsHeld.store(mSlotsHeldLocked);
		item.mState.store(GateItem::State::kDone);
		PumpLocked();
		mDoneCv.notify_all();
	}

	AdmitResult CapGate::Admit(GateItem& item)
	{
		const uint64_t now = NowNs();
		item.mEnqueueNs = now;
		item.mAdmitNs = 0;

		std::unique_lock<std::mutex> lock(mMutex);
		if (mSlotsHeldLocked < mWorkerCap && mCount == 0)
		{
			item.mAdmitNs = now;
			return SubmitLocked(item) ? AdmitResult::kSubmitted : AdmitResult::kSubmitFailed;
		}

		if (mCount == mQueue.size())
		{
			lock.unlock();
			DIA_ASSERT(false, "CapGate queue full; running item inline");
			item.mState.store(GateItem::State::kRunning);
			item.mWork();
			lock.lock();
			item.mState.store(GateItem::State::kDone);
			mDoneCv.notify_all();
			return AdmitResult::kSubmitted;
		}

		item.mState.store(GateItem::State::kQueued);
		EnqueueLocked(&item);
		mQueuedAtLeastOnce.fetch_add(1);
		return AdmitResult::kQueued;
	}

	bool CapGate::TrySteal(GateItem& item)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (item.mState.load() != GateItem::State::kQueued)
		{
			return false;
		}
		const size_t cap = mQueue.size();
		size_t found = mCount;
		for (size_t i = 0; i < mCount; ++i)
		{
			if (mQueue[(mHead + i) % cap] == &item) { found = i; break; }
		}
		if (found == mCount)
		{
			return false;   // defensive: state said queued but it is not in the ring
		}
		for (size_t i = found; i + 1 < mCount; ++i)
		{
			mQueue[(mHead + i) % cap] = mQueue[(mHead + i + 1) % cap];
		}
		mQueue[(mHead + mCount - 1) % cap] = nullptr;
		--mCount;
		item.mState.store(GateItem::State::kStolen);
		mStolen.fetch_add(1);
		return true;
	}

	GateItem::State CapGate::WaitItem(GateItem& item)
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mDoneCv.wait(lock, [&item]
		{
			const GateItem::State s = item.mState.load();
			return s == GateItem::State::kDone || s == GateItem::State::kSubmitFailed || s == GateItem::State::kStolen;
		});
		return item.mState.load();
	}
}
