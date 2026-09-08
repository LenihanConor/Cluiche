////////////////////////////////////////////////////////////////////////////////
// Filename: CameraRegistryHealth3D.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Camera3D { class CameraRegistry3D; } }

namespace Dia
{
	namespace Camera3D
	{
		class CameraRegistryHealth3D : public Dia::Observation::Health::IHealthReporter
		{
		public:
			explicit CameraRegistryHealth3D(const CameraRegistry3D& registry)
				: mRegistry(registry) {}

			Dia::Core::StringCRC GetReporterName() const override
			{
				return Dia::Core::StringCRC("dia.camera3d.registry");
			}

			Dia::Observation::Health::Health Report() const override;

		private:
			const CameraRegistry3D& mRegistry;
		};

	} // namespace Camera3D
} // namespace Dia
