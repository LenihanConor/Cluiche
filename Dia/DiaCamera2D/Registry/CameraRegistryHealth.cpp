////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistryHealth.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera2D/Registry/CameraRegistryHealth.h"
#include "DiaCamera2D/Registry/CameraRegistry2D.h"

namespace Dia
{
	namespace Camera2D
	{
		Dia::Observation::Health::Health CameraRegistryHealth::Report() const
		{
			Dia::Observation::Health::Health h;
			h.errors   = 0;
			h.warnings = 0;

			if (mRegistry.GetCount() == 0)
			{
				h.status = Dia::Observation::Health::HealthStatus::kDegraded;
				h.reason = Dia::Core::StringCRC("no cameras registered");
				h.warnings = 1;
			}
			else
			{
				h.status = Dia::Observation::Health::HealthStatus::kOK;
				h.reason = Dia::Core::StringCRC("");
			}

			return h;
		}

	} // namespace Camera2D
} // namespace Dia
