#include "PerfTimingStats.h"

#include "CoarseRing.h"

#include <algorithm>
#include <cmath>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		void ComputePerfSeriesStats(const uint16_t* fixed, const uint8_t* flags, uint32_t count,
			PerfSeriesStats& out, std::vector<uint16_t>& sortedScratch)
		{
			out = PerfSeriesStats();
			if (fixed == nullptr || count == 0)
				return;

			sortedScratch.clear();
			float ewma = 0.0f;
			for (uint32_t i = 0; i < count; ++i)
			{
				if (flags != nullptr && (flags[i] & CoarseRing::kFlagGap) != 0)
					continue;
				const float ms = CoarseRing::FromFixed(fixed[i]);
				ewma = sortedScratch.empty() ? ms : ewma + kPerfTimingEwmaAlpha * (ms - ewma);
				sortedScratch.push_back(fixed[i]);
			}
			if (sortedScratch.empty())
				return;

			std::sort(sortedScratch.begin(), sortedScratch.end());
			const uint32_t n    = static_cast<uint32_t>(sortedScratch.size());
			const uint32_t rank = static_cast<uint32_t>(std::ceil(0.95 * static_cast<double>(n)));
			out.samples = n;
			out.ewmaMs  = ewma;
			out.p95Ms   = CoarseRing::FromFixed(sortedScratch[(rank == 0 ? 1 : rank) - 1]);
		}

		bool ComputePerfRingSeriesStats(const CoarseRing& ring, uint32_t series, float unitHz, float seconds,
			PerfSeriesStats& out, PerfTimingScratch& scratch)
		{
			out = PerfSeriesStats();
			const uint64_t total = ring.GetEntryCount();
			if (ring.GetCapacity() == 0 || total == 0)
				return false;

			float wanted = (unitHz > 0.0f ? unitHz : 60.0f) * seconds;
			if (!(wanted >= 1.0f)) wanted = 1.0f;
			uint32_t n = wanted > static_cast<float>(kPerfTimingMaxEntries) ? kPerfTimingMaxEntries : static_cast<uint32_t>(wanted + 0.5f);
			if (static_cast<uint64_t>(n) > total) n = static_cast<uint32_t>(total);

			uint64_t first = total - n;
			const uint64_t oldest = ring.GetOldestAvailable();
			if (first < oldest)
			{
				first = oldest;
				n = static_cast<uint32_t>(total - first);
			}
			if (n == 0)
				return false;

			scratch.values.resize(n);
			scratch.flags.resize(n);
			const uint32_t got = ring.CopyRange(series, first, n, scratch.values.data());
			const uint32_t gotFlags = ring.CopyFlags(first, n, scratch.flags.data());
			if (got == 0)
				return false;
			ComputePerfSeriesStats(scratch.values.data(), gotFlags == got ? scratch.flags.data() : nullptr, got, out, scratch.sorted);
			return out.samples > 0;
		}
	}}
}
