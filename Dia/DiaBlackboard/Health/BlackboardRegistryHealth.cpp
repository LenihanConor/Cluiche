////////////////////////////////////////////////////////////////////////////////
// Filename: BlackboardRegistryHealth.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBlackboard/Health/BlackboardRegistryHealth.h"
#include "DiaBlackboard/BlackboardRegistry.h"

namespace Dia
{
	namespace Blackboard
	{
		Dia::Observation::Health::Health BlackboardRegistryHealth::Report() const
		{
			Dia::Observation::Health::Health h;
			h.errors   = 0;
			h.warnings = 0;

			const int count = mRegistry.GetCount();

			if (count >= 16)
			{
				h.status  = Dia::Observation::Health::HealthStatus::kFailing;
				h.reason  = Dia::Core::StringCRC("blackboard registry at capacity — overflow imminent");
				h.errors  = 1;
			}
			else if (count >= 12)
			{
				h.status   = Dia::Observation::Health::HealthStatus::kDegraded;
				h.reason   = Dia::Core::StringCRC("blackboard registry 75% full");
				h.warnings = 1;
			}
			else
			{
				h.status = Dia::Observation::Health::HealthStatus::kOK;
				h.reason = Dia::Core::StringCRC("");
			}

			return h;
		}

	} // namespace Blackboard
} // namespace Dia
