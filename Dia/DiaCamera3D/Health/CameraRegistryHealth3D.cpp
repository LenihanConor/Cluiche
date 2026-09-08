////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistryHealth3D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCamera3D/Health/CameraRegistryHealth3D.h"
#include "DiaCamera3D/Registry/CameraRegistry3D.h"

namespace Dia
{
	namespace Camera3D
	{
		Dia::Observation::Health::Health CameraRegistryHealth3D::Report() const
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
			else if (!mRegistry.HasActiveCamera())
			{
				h.status = Dia::Observation::Health::HealthStatus::kDegraded;
				h.reason = Dia::Core::StringCRC("no active camera set");
				h.warnings = 1;
			}
			else
			{
				h.status = Dia::Observation::Health::HealthStatus::kOK;
				h.reason = Dia::Core::StringCRC("");
			}

			return h;
		}

	} // namespace Camera3D
} // namespace Dia
