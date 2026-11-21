#pragma once

#include <DiaDeferredStep/IParallelExecutor.h>

namespace Dia::DeferredStep
{
	// One range, on the calling thread. Tests, and the fallback when no job system exists.
	class SerialExecutor final : public IParallelExecutor
	{
	public:
		void     ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn) override;
		uint32_t GetLastRangeCount() const override;

	private:
		uint32_t mLastRangeCount = 0;
	};
}
