#pragma once

#include <stdint.h>

namespace Dia::DeferredStep
{
	// Pure arithmetic: clamp(ceil(count / minRange), 1, workerCap * rangesPerWorker).
	// minRange 0 is treated as 1; count 0 yields 1; the cap product is computed in 64 bits and is at least 1.
	inline uint32_t ComputeRangeCount(uint32_t count, uint32_t minRange, uint32_t workerCap, uint32_t rangesPerWorker)
	{
		const uint64_t minR = minRange == 0 ? 1u : minRange;
		uint64_t maxRanges = static_cast<uint64_t>(workerCap) * static_cast<uint64_t>(rangesPerWorker);
		if (maxRanges < 1) { maxRanges = 1; }
		if (maxRanges > 0xFFFFFFFFull) { maxRanges = 0xFFFFFFFFull; }
		uint64_t ranges = (static_cast<uint64_t>(count) + minR - 1) / minR;
		if (ranges < 1) { ranges = 1; }
		if (ranges > maxRanges) { ranges = maxRanges; }
		return static_cast<uint32_t>(ranges);
	}

	// Half-open [outBegin, outEnd) for range `index` of `rangeCount`. The first (count % rangeCount) ranges get
	// one extra element. rangeCount 0 is treated as 1. Indices >= rangeCount yield an empty range at count.
	inline void GetRangeBounds(uint32_t count, uint32_t rangeCount, uint32_t index, uint32_t& outBegin, uint32_t& outEnd)
	{
		const uint32_t n = rangeCount == 0 ? 1u : rangeCount;
		if (index >= n)
		{
			outBegin = count;
			outEnd = count;
			return;
		}
		const uint32_t baseSize = count / n;
		const uint32_t extra = count % n;
		const uint32_t extraBefore = index < extra ? index : extra;
		outBegin = index * baseSize + extraBefore;
		outEnd = outBegin + baseSize + (index < extra ? 1u : 0u);
	}
}
