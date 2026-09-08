#pragma once

#include <DiaObservation/Config/ObservationConfig.h>

namespace Dia
{
	namespace Observation
	{
		class ObservationConfigCli
		{
		public:
			static void ApplyOverrides(int argc, const char* const* argv, ObservationConfig& config);
		};

	} // namespace Observation
} // namespace Dia
