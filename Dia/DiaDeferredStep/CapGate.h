#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdint.h>
#include <vector>
#include <DiaDeferredStep/CapConfig.h>

namespace Dia::Threading { class JobSystem; }

namespace Dia::DeferredStep
{
	// Plain snapshot of the gate's counters. GetStats() returns it by value.
	struct CapStats
	{
		uint32_t slotsHeld         = 0;   // now
		uint32_t peakSlotsHeld     = 0;
		uint32_t workerCap         = 0;   // resolved M
		uint64_t admitted          = 0;
		uint64_t queuedAtLeastOnce = 0;
		uint64_t stolen            = 0;
		uint64_t trySubmitFailed   = 0;
		uint64_t totalCapWaitNs    = 0;
		uint64_t peakQueueDepth    = 0;
	};

	//------------------------------------------------------------------------------------------
	// INTERNAL to DiaDeferredStep: GateItem, AdmitResult and CapGate's admission API
	// (Admit / TrySteal / WaitItem / AddCapWait / SetSubmitFailureHookForTest) are not part of the
	// public contract. Consumers only see IParallelExecutor and IStepLauncher.
	//------------------------------------------------------------------------------------------

	// A unit of work admitted through the gate. Owned and kept alive by the caller until it reads
	// kDone, kStolen (it ran the work itself) or kSubmitFailed (see contract below). Reusable via
	// Prepare() once finished. Not copyable.
	//
	// State machine:  kIdle -> (Admit) -> kRunning -> kDone
	//                                  -> kQueued  -> kRunning -> kDone          (pumped)
	//                                              -> kStolen                    (TrySteal)
	//                                              -> kSubmitFailed              (pump-time TrySubmit failure)
	//                          kIdle   -> kSubmitFailed                          (admit-time TrySubmit failure)
	class GateItem
	{
	public:
		enum class Kind  : uint32_t { kCoordinator, kRange };
		enum class State : uint32_t { kIdle, kQueued, kRunning, kDone, kStolen, kSubmitFailed };

		GateItem() = default;
		GateItem(const GateItem&) = delete;
		GateItem& operator=(const GateItem&) = delete;

		// Only legal when the item is not queued/running.
		void Prepare(Kind kind, std::function<void()> work)
		{
			mKind = kind;
			mWork = std::move(work);
			mEnqueueNs = 0;
			mAdmitNs = 0;
			mState.store(State::kIdle);
		}

		State    GetState()     const { return mState.load(); }
		Kind     GetKind()      const { return mKind; }
		// Valid once the item has been admitted/stolen/failed (read after observing the state).
		uint64_t GetEnqueueNs() const { return mEnqueueNs; }
		uint64_t GetAdmitNs()   const { return mAdmitNs; }   // 0 until a slot was granted

		// For the thread that obtained the item by TrySteal or by kSubmitFailed: run the work
		// inline. Does not touch the gate (no slot is taken or released).
		void RunInline() { if (mWork) { mWork(); } }

	private:
		friend class CapGate;

		std::function<void()> mWork;
		Kind                  mKind = Kind::kRange;
		uint64_t              mEnqueueNs = 0;
		uint64_t              mAdmitNs = 0;
		std::atomic<State>    mState{ State::kIdle };
	};

	enum class AdmitResult : uint32_t
	{
		kSubmitted,       // a slot was taken and the item was handed to the JobSystem
		kQueued,          // gate full; admitted later by whichever thread releases a slot
		kSubmitFailed     // TrySubmit failed; no slot held; item state is kSubmitFailed
	};

	// Admission control shared by every channel. Thread-safe: workers release slots and pump
	// the queue; SimPU admits coordinators.
	//
	// Strict M: the number of admitted-and-unfinished items never exceeds GetWorkerCap(). An
	// item's slot is held for the duration of its work and released by the same job wrapper, which
	// then pumps the FIFO queue. Admit never blocks.
	//
	// kSubmitFailed contract: the item holds NO slot. Whoever waits on it (WaitItem returns
	// kSubmitFailed) is responsible for running it inline via RunInline(). Running it inline sits
	// outside the strict-M accounting; it is the exceptional fallback (pool absent / shut down).
	//
	// Queue-full contract: the queue is sized for kMaxChannels * (M * rangesPerWorker + 1) items.
	// If that is ever exceeded, DIA_ASSERT fires and the item is run inline on the Admit caller
	// (never dropped); it is reported as kSubmitted with state kDone.
	class CapGate
	{
	public:
		static const uint32_t kMaxChannelsHint = 8;   // mirrors DeferredStepScheduler::kMaxChannels

		CapGate(Dia::Threading::JobSystem& jobs, const CapConfig& config);
		~CapGate();                                   // asserts the gate is empty
		CapGate(const CapGate&) = delete;
		CapGate& operator=(const CapGate&) = delete;

		uint32_t GetWorkerCap() const { return mWorkerCap; }   // resolved M
		uint32_t GetRangesPerWorker() const { return mRangesPerWorker; }   // config value, min 1
		CapStats GetStats() const;                              // snapshot by value (atomics inside)
		void     ResetStats();                                  // keeps workerCap; peaks restart from current

		// --- internal admission API ---
		AdmitResult Admit(GateItem& item);
		// True if the item was still queued: it is removed, marked kStolen and the caller must run
		// it inline (no slot). False if it is running, done or was never queued.
		bool        TrySteal(GateItem& item);
		// Blocks until the item is kDone or kSubmitFailed; returns that state.
		GateItem::State WaitItem(GateItem& item);
		// Executors add startNs - enqueueNs for stolen items here.
		void        AddCapWait(uint64_t ns) { mTotalCapWaitNs.fetch_add(ns); }

		// TEST ONLY: when set, called (under the gate lock) before each TrySubmit; returning true
		// forces that submit to be treated as failed. Set before any item is admitted.
		void        SetSubmitFailureHookForTest(std::function<bool()> shouldFail) { mFailureHook = std::move(shouldFail); }

	private:
		bool SubmitLocked(GateItem& item);               // lock held; takes a slot; false = failed (slot returned)
		void Finish(GateItem& item);                     // job wrapper epilogue: release + pump
		void PumpLocked();
		void EnqueueLocked(GateItem* item);

		Dia::Threading::JobSystem& mJobs;
		uint32_t                   mWorkerCap = 1;
		uint32_t                   mRangesPerWorker = 1;

		mutable std::mutex         mMutex;
		std::condition_variable    mDoneCv;
		std::vector<GateItem*>     mQueue;               // fixed-capacity ring
		size_t                     mHead = 0;
		size_t                     mCount = 0;
		uint32_t                   mSlotsHeldLocked = 0;
		std::function<bool()>      mFailureHook;

		std::atomic<uint32_t>      mSlotsHeld{ 0 };
		std::atomic<uint32_t>      mPeakSlotsHeld{ 0 };
		std::atomic<uint64_t>      mAdmitted{ 0 };
		std::atomic<uint64_t>      mQueuedAtLeastOnce{ 0 };
		std::atomic<uint64_t>      mStolen{ 0 };
		std::atomic<uint64_t>      mTrySubmitFailed{ 0 };
		std::atomic<uint64_t>      mTotalCapWaitNs{ 0 };
		std::atomic<uint64_t>      mPeakQueueDepth{ 0 };
	};
}
