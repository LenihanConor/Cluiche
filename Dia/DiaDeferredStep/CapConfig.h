#pragma once

#include <stdint.h>
#include <stddef.h>

namespace Dia::DeferredStep
{
	struct CapConfig
	{
		// 0 means derive: max(1, workerCount / 4) (system spec Worker cap).
		uint32_t workerCap       = 0;
		// Upper bound on ranges per ParallelFor, as a multiple of workerCap.
		uint32_t rangesPerWorker = 4;
	};

	// Pure arithmetic. workerCount 0 is treated as 1. An explicit cap above workerCount is clamped to
	// workerCount and *outClamped is set (the Warning log is the caller's job). outClamped may be null.
	inline uint32_t ResolveWorkerCap(const CapConfig& config, size_t workerCount, bool* outClamped)
	{
		const uint32_t workers = workerCount == 0 ? 1u : static_cast<uint32_t>(workerCount);
		bool clamped = false;
		uint32_t cap;
		if (config.workerCap == 0)
		{
			cap = workers / 4;
			if (cap < 1) { cap = 1; }
		}
		else if (config.workerCap > workers)
		{
			cap = workers;
			clamped = true;
		}
		else
		{
			cap = config.workerCap;
		}
		if (outClamped) { *outClamped = clamped; }
		return cap;
	}
}
