#pragma once

#include <stdint.h>
#include <functional>

namespace Dia::ParallelExecutor
{
	// fn(begin, end) over a half-open sub-range of [0, count).
	using RangeFn = std::function<void(uint32_t begin, uint32_t end)>;

	class IParallelExecutor
	{
	public:
		virtual ~IParallelExecutor() = default;

		// Splits [0, count) into ranges of at least minRange and calls fn on each. Returns only when every
		// range has finished. May run ranges on the calling thread. Callers guarantee ranges are independent.
		virtual void ParallelFor(uint32_t count, uint32_t minRange, const RangeFn& fn) = 0;

		// Ranges the last ParallelFor split into; for tests and metrics.
		virtual uint32_t GetLastRangeCount() const = 0;
	};
}
