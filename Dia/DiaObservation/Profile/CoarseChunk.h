#pragma once

#include <DiaObservation/Profile/CoarseRing.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// POD payload for per-PU coarse timing chunks (~8.5 KB). Sent via EventStream
		// from each PU to Main at ~10 Hz. Trivially copyable for efficient streaming.
		struct CoarseChunk
		{
			static constexpr uint32_t kMaxEntries = 64;
			static constexpr uint32_t kSeriesCount = CoarseRing::kMaxModules + 1;

			uint32_t unitCrc;                                         // ProcessingUnit instanceId CRC
			uint32_t seq;                                             // chunk sequence number (increments per Take)
			uint64_t firstEntry;                                      // absolute entry index of first entry in this chunk
			uint32_t count;                                           // entries in this chunk (1..kMaxEntries)
			uint32_t moduleCount;                                     // modules recorded (0..kMaxModules)
			float    unitHz;                                          // unit frequency (for ring capacity calculation on Main)
			uint32_t moduleCrcs[CoarseRing::kMaxModules];             // per-module CRCs
			uint16_t ms[kSeriesCount][kMaxEntries];                   // series-major: ms[series][entry]; unit tick at index kMaxModules
			uint8_t  flags[kMaxEntries];                              // per-entry flags (kFlagClamped, kFlagGap)
			uint32_t tripCount;                                       // cumulative trip count (if reachable)
			uint32_t markerDrops;                                     // cumulative marker drops (TODO)
		};

		static_assert(sizeof(CoarseChunk) <= 9000, "CoarseChunk exceeds ~8.5 KB budget");
	}}
}
