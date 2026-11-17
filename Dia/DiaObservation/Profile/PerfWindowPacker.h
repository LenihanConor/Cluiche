#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		class CoarseRing;

		// Same shape as TripNameResolver: writes/returns a name for the crc, nullptr or a
		// resolver that returns nullptr falls back to "0x%08x".
		typedef const char* (*PerfNameResolver)(uint32_t crc, char* buf, size_t bufSize);

		struct PerfWindowUnitInput
		{
			uint32_t          unitCrc;
			const char*       name;
			float             unitHz;
			const CoarseRing* ring;     // may be nullptr (packed as an empty unit)
		};

		struct PerfFrameSection
		{
			const char* name;
			float       ms;
		};

		// perf.window payload. Timing arrays are base64 of little-endian uint16 (0.01 ms fixed
		// point), one JSON string per array; moduleMs is module-major. seconds is clamped to [1, 60].
		void PackPerfWindow(const PerfWindowUnitInput* units, uint32_t unitCount, float seconds,
			PerfNameResolver resolver, std::string& outJson);

		// perf.frame payloads.
		void PackPerfFrameUnavailable(std::string& outJson);
		void PackPerfFrameSections(const char* unit, uint64_t frame, const PerfFrameSection* sections,
			uint32_t count, std::string& outJson);
	}}
}
