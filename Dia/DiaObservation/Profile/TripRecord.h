#pragma once

#include <DiaObservation/Profile/TripDetector.h>
#include <DiaObservation/Profile/UnitTimingTable.h>

#include <cstddef>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		// Fixed-size POD describing one emitted over-budget trip (amendment A3).
		// Built on the PU thread (no allocation, no I/O), serialised on the drain thread.
		struct TripRecord
		{
			static constexpr uint32_t kUnitLen    = 32;
			static constexpr uint32_t kNameLen    = 48;
			static constexpr uint32_t kConfigLen  = 12;
			static constexpr uint32_t kTopCount   = 5;
			static constexpr uint32_t kSections   = 5;
			static constexpr uint32_t kRecent     = UnitTimingTable::kRecentMax;
			static constexpr uint32_t kMaxModules = UnitTimingTable::kMaxModules;

			struct NamedMs
			{
				char  name[kNameLen];
				float ms;
			};

			char     unit[kUnitLen];
			char     buildConfig[kConfigLen];
			uint32_t firstFrame;
			uint32_t lastFrame;
			float    budgetMs;
			float    peakMs;
			uint32_t suppressedSinceLastTrip;

			uint32_t topCount;                 // <= kTopCount, descending by mean ms over the trip window
			NamedMs  top[kTopCount];

			// "With sections" hook: top profiler scopes of the worst frame. Only to be filled when profiling
			// is on and the data is cheaply available; the builder leaves it empty (sectionCount == 0).
			uint32_t sectionCount;
			NamedMs  sections[kSections];

			uint32_t worstIndex;               // index into recent[] of the highest unit tick (valid if recentCount > 0)
			float    worstUnitMs;
			uint32_t worstCount;               // modules in the worst frame breakdown
			NamedMs  worst[kMaxModules];

			uint32_t recentCount;              // <= kRecent, oldest first, newest last
			float    recent[kRecent];
		};

		// Resolves a module crc to text. nullptr = Profiler::ResolveSubjectName (hex fallback when unknown).
		typedef const char* (*TripNameResolver)(uint32_t crc, char* buf, size_t bufSize);

		// Pure builder. Mean over the trip window = the min(rangeLength, ring, available) most recent samples.
		void BuildTripRecord(TripRecord& out, const char* unit, const TripResult& trip, float budgetMs,
		                     const char* buildConfig, const UnitTimingTable& table,
		                     TripNameResolver resolver = nullptr);

		// One JSON object on a single line, terminated by a newline.
		void SerializeTripRecord(const TripRecord& record, char* outBuf, size_t outSize, size_t& outLen);
	}}
}
