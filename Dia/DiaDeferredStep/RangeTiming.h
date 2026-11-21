#pragma once

#include <stdint.h>

namespace Dia::DeferredStep
{
	// RangeTiming::flags bits.
	static constexpr uint32_t kRangeStolen          = 1;
	static constexpr uint32_t kRangeTrySubmitFailed = 2;

	// Per-range timing record. Plain struct; no observation dependency.
	struct RangeTiming
	{
		uint64_t enqueueNs = 0;
		uint64_t admitNs   = 0;
		uint64_t startNs   = 0;
		uint64_t endNs     = 0;
		uint64_t capWaitNs = 0;
		uint32_t flags     = 0;
	};
}
