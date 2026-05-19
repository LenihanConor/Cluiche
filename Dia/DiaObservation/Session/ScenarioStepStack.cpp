#include "DiaObservation/Session/ScenarioStepStack.h"

namespace Dia
{
	namespace Observation
	{
		struct ThreadStepStack
		{
			Dia::Core::StringCRC steps[ScenarioStepStack::kMaxDepth];
			unsigned int depth = 0;
		};

		static thread_local ThreadStepStack tStepStack;

		void ScenarioStepStack::Push(const Dia::Core::StringCRC& step)
		{
			if (tStepStack.depth < kMaxDepth)
			{
				tStepStack.steps[tStepStack.depth] = step;
				++tStepStack.depth;
			}
		}

		void ScenarioStepStack::Pop()
		{
			if (tStepStack.depth > 0)
				--tStepStack.depth;
		}

		Dia::Core::StringCRC ScenarioStepStack::Current()
		{
			if (tStepStack.depth > 0)
				return tStepStack.steps[tStepStack.depth - 1];
			return Dia::Core::StringCRC();
		}
	}
} // namespace Dia
