#pragma once

#include <DiaObservation/Config/ObservationConfig.h>

namespace Dia
{
	namespace Observation
	{
		class ObservationConfigLoader
		{
		public:
			static bool Load(const char* diagamePath, ObservationConfig& config);
			static bool LoadFromString(const char* jsonString, ObservationConfig& config);

		private:
			static LogLevelConfig ParseLevel(const char* str, LogLevelConfig fallback);
		};

	} // namespace Observation
} // namespace Dia
