#pragma once

#include <DiaDeferredStep/DeferredStepTypes.h>

namespace Dia::DeferredStep
{
	// Implemented by the consumer's clock. Dia never interprets the value; it only requires it to be monotonic.
	class IStepKeySource
	{
	public:
		virtual ~IStepKeySource() = default;
		virtual StepKey GetCurrentKey() const = 0;
	};
}
