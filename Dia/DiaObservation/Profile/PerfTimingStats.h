#pragma once

#include <cstdint>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		class CoarseRing;

		// Perf Inspector perf.timing: live EWMA and p95 of one coarse series over a short recent window.
		//
		// Rules (pinned by tests):
		//  - Entries flagged CoarseRing::kFlagGap are gap fillers (no measurement): IGNORED, not counted.
		//  - Clamped entries (value 65535, over 655.35 ms) are counted at their stored value, 655.35 ms.
		//  - p95 is NEAREST RANK on the ascending sorted values: rank = ceil(0.95 * n), result = sorted[rank - 1]
		//    (the same method as LatencyProbe).
		//  - EWMA runs oldest to newest, seeded with the first sample, ewma += alpha * (x - ewma).
		//  - No samples: samples == 0, ewmaMs == p95Ms == 0 (callers treat samples == 0 as "no data").
		struct PerfSeriesStats
		{
			uint32_t samples = 0;
			float    ewmaMs  = 0.0f;
			float    p95Ms   = 0.0f;
		};

		static constexpr float    kPerfTimingEwmaAlpha   = 0.2f;
		static constexpr float    kPerfTimingWindowSecs  = 2.0f;
		static constexpr uint32_t kPerfTimingMaxEntries  = 1024;   // cap for very high unit rates

		// Reusable buffers so the 10 Hz caller does not allocate once warm.
		struct PerfTimingScratch
		{
			std::vector<uint16_t> values;
			std::vector<uint8_t>  flags;
			std::vector<uint16_t> sorted;
		};

		// Pure: fixed-point (0.01 ms) values, optional per-entry flags (may be null), oldest first.
		void ComputePerfSeriesStats(const uint16_t* fixed, const uint8_t* flags, uint32_t count,
			PerfSeriesStats& out, std::vector<uint16_t>& sortedScratch);

		// Stats of one series (module slot or CoarseRing::kUnitSlot) over the newest ~seconds of the ring.
		// Returns false (out cleared) when the ring is empty or inert.
		bool ComputePerfRingSeriesStats(const CoarseRing& ring, uint32_t series, float unitHz, float seconds,
			PerfSeriesStats& out, PerfTimingScratch& scratch);
	}}
}
