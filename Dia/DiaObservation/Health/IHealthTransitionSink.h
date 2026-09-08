#pragma once

#include <DiaObservation/Health/HealthRegistry.h>

namespace Dia
{
	namespace Observation { namespace Health
	{
		class IHealthTransitionSink
		{
		public:
			virtual ~IHealthTransitionSink() = default;
			virtual void OnTransition(const HealthRegistry::Transition& transition) = 0;
		};
	}
} // namespace Observation
} // namespace Dia
