////////////////////////////////////////////////////////////////////////////////
// Filename: BlackboardRegistryHealth.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Blackboard { class BlackboardRegistry; } }

namespace Dia
{
	namespace Blackboard
	{
		class BlackboardRegistryHealth : public Dia::Observation::Health::IHealthReporter
		{
		public:
			explicit BlackboardRegistryHealth(const BlackboardRegistry& registry)
				: mRegistry(registry) {}

			Dia::Core::StringCRC GetReporterName() const override
			{
				return Dia::Core::StringCRC("dia.blackboard.registry");
			}

			Dia::Observation::Health::Health Report() const override;

		private:
			const BlackboardRegistry& mRegistry;
		};

	} // namespace Blackboard
} // namespace Dia
