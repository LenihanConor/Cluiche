#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Health
	{
		enum class HealthStatus : uint8_t
		{
			kOK,
			kDegraded,
			kFailing
		};

		struct Health
		{
			HealthStatus         status;
			unsigned int         errors;
			unsigned int         warnings;
			Dia::Core::StringCRC reason;
		};

		class IHealthReporter
		{
		public:
			virtual ~IHealthReporter() = default;
			virtual Dia::Core::StringCRC GetReporterName() const = 0;
			virtual Health               Report() const = 0;
		};

		inline const char* HealthStatusToString(HealthStatus s)
		{
			switch (s)
			{
			case HealthStatus::kOK:       return "ok";
			case HealthStatus::kDegraded: return "degraded";
			case HealthStatus::kFailing:  return "failing";
			}
			return "unknown";
		}
	}
} // namespace Observation
} // namespace Dia
