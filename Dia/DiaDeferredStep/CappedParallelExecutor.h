#pragma once

#include <stdint.h>
#include <vector>
#include <DiaDeferredStep/DeferredStepTypes.h>
#include <DiaDeferredStep/IParallelExecutor.h>
#include <DiaDeferredStep/RangeTiming.h>

namespace Dia::DeferredStep
{
	class CapGate;

	// INTERNAL to DiaDeferredStep. Short-lived per-step executor. Not thread-safe: only the coordinator
	// thread (the one running the step's Run, already inside a gate slot) calls ParallelFor.
	// (Tests may call it from a plain thread; ranges then run without the caller holding a slot.)
	//
	// Core invariant (no stranded range): the coordinator runs range 0 itself, then TrySteal()s every
	// range still queued behind the cap and runs it inline, and only waits (WaitItem) on ranges that
	// were already admitted (running on a worker). A range can therefore never sit queued behind a cap
	// that the coordinator itself is filling, so M = 1 on a one-worker pool still completes.
	//
	// ParallelFor returns only after every range has finished. The first exception thrown by a range
	// body is captured, all other ranges still finish, then it is rethrown.
	class CappedParallelExecutor : public IParallelExecutor
	{
	public:
		CappedParallelExecutor(CapGate& gate, const LaunchInfo& info);
		~CappedParallelExecutor() override = default;
		CappedParallelExecutor(const CappedParallelExecutor&) = delete;
		CappedParallelExecutor& operator=(const CappedParallelExecutor&) = delete;

		void     ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn) override;
		uint32_t GetLastRangeCount() const override { return mLastRangeCount; }

		// Timings reflect the LAST ParallelFor call only (the array is rebuilt per call).
		uint32_t           GetRangeTimingCount() const { return static_cast<uint32_t>(mTimings.size()); }
		const RangeTiming& GetRangeTimingAt(uint32_t index) const { return mTimings[index]; }
		// Cumulative number of range bodies run across every ParallelFor call on this executor.
		uint32_t           GetTotalRangesRun() const { return mTotalRangesRun; }

		// TEST ONLY: overrides the computed range count (0 = off). Still capped to count when count > 0.
		void SetForcedRangeCount(uint32_t rangeCount) { mForcedRangeCount = rangeCount; }

	private:
		CapGate&                 mGate;
		LaunchInfo               mInfo;
		uint32_t                 mLastRangeCount = 1;
		uint32_t                 mForcedRangeCount = 0;
		uint32_t                 mTotalRangesRun = 0;
		std::vector<RangeTiming> mTimings;
	};
}
