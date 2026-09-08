#include "DiaObservation/Session/SessionIdGenerator.h"

#include <chrono>
#include <ctime>
#include <cstdio>
#include <random>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Dia
{
	namespace Observation { namespace Internal
	{
		void GenerateSessionId(char (&out)[32])
		{
			auto now = std::chrono::system_clock::now();
			std::time_t tt = std::chrono::system_clock::to_time_t(now);

			struct tm utc;
			gmtime_s(&utc, &tt);

			auto seed = static_cast<unsigned int>(
				std::chrono::steady_clock::now().time_since_epoch().count()
				^ static_cast<uint64_t>(GetCurrentProcessId()));
			std::mt19937 rng(seed);
			uint32_t suffix = rng();

			snprintf(out, 32, "%04d%02d%02d-%02d%02d%02d-%08x",
				utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
				utc.tm_hour, utc.tm_min, utc.tm_sec,
				suffix);
		}
	}
} // namespace Observation
} // namespace Dia
