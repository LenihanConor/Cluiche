#pragma once

#include <stddef.h>
#include <initializer_list>
#include <DiaDeferredStep/DeferredStepTypes.h>

namespace Dia::DeferredStep::Testing
{
	// Drives a scheduler through a scripted key list, one Tick(key) per entry, so jumps and repeats are data.
	// Scheduler only needs a Tick(StepKey) method.
	class ScriptedKeyDriver
	{
	public:
		template<class Scheduler>
		static void Run(Scheduler& scheduler, const StepKey* keys, size_t count)
		{
			for (size_t i = 0; i < count; ++i)
			{
				scheduler.Tick(keys[i]);
			}
		}

		template<class Scheduler>
		static void Run(Scheduler& scheduler, std::initializer_list<StepKey> keys)
		{
			for (StepKey key : keys)
			{
				scheduler.Tick(key);
			}
		}
	};
}
