#pragma once

#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Health/HealthReporterBase.h>

namespace Dia
{
	namespace Observation { namespace Testing
	{
		class HealthFixture
		{
		public:
			HealthFixture()  { Health::HealthRegistry::Instance().Reset(); }
			~HealthFixture() { Health::HealthRegistry::Instance().Reset(); }
		};

		class MockHealthReporter : public Health::HealthReporterBase
		{
		public:
			explicit MockHealthReporter(const Dia::Core::StringCRC& name)
				: mName(name)
			{
			}

			Dia::Core::StringCRC GetReporterName() const override { return mName; }

		private:
			Dia::Core::StringCRC mName;
		};
	}
} // namespace Observation
} // namespace Dia
