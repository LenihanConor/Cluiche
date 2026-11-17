#include <DiaDeferredStep/SerialExecutor.h>

namespace Dia::DeferredStep
{
	void SerialExecutor::ParallelFor(uint32_t count, uint32_t /*minRange*/, const RangeFn& fn)
	{
		if (count == 0)
		{
			mLastRangeCount = 0;
			return;
		}

		mLastRangeCount = 1;
		fn(0, count);
	}

	uint32_t SerialExecutor::GetLastRangeCount() const
	{
		return mLastRangeCount;
	}
}
