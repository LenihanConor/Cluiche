#pragma once

#include <stdint.h>
#include <DiaDeferredStep/IParallelExecutor.h>
#include <DiaDeferredStep/RangeSplit.h>

namespace Dia::DeferredStep::Testing
{
	// Test executor: splits exactly like the capped executor (same range maths) but runs the ranges
	// single-threaded in REVERSE order. A step whose result changes under this executor depends on range order.
	class ReorderingExecutor final : public IParallelExecutor
	{
	public:
		// forcedRangeCount 0 = derive from count/minRange with an 8 x 4 range ceiling.
		explicit ReorderingExecutor(uint32_t forcedRangeCount = 0) : mForcedRangeCount(forcedRangeCount) {}

		void ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn) override
		{
			uint32_t ranges = mForcedRangeCount != 0 ? mForcedRangeCount : ComputeRangeCount(count, minRange, 8, 4);
			if (count > 0 && ranges > count) { ranges = count; }
			if (ranges < 1) { ranges = 1; }
			mLastRangeCount = ranges;
			for (uint32_t r = ranges; r-- > 0;)
			{
				uint32_t b = 0;
				uint32_t e = 0;
				GetRangeBounds(count, ranges, r, b, e);
				fn(b, e);
			}
		}

		uint32_t GetLastRangeCount() const override { return mLastRangeCount; }

	private:
		uint32_t mForcedRangeCount = 0;
		uint32_t mLastRangeCount = 1;
	};
}
