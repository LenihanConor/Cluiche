////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistryHealth.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Camera2D { class CameraRegistry2D; } }

namespace Dia
{
	namespace Camera2D
	{
		class CameraRegistryHealth : public Dia::Observation::Health::IHealthReporter
		{
		public:
			explicit CameraRegistryHealth(const CameraRegistry2D& registry)
				: mRegistry(registry) {}

			Dia::Core::StringCRC GetReporterName() const override
			{
				return Dia::Core::StringCRC("dia.camera2d.registry");
			}

			Dia::Observation::Health::Health Report() const override;

		private:
			const CameraRegistry2D& mRegistry;
		};

	} // namespace Camera2D
} // namespace Dia
