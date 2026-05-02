#pragma once

#include "DiaStateMachine/PushdownAutomatonDefinition.h"

namespace Dia
{
	namespace StateMachine
	{
		class PushdownAutomatonBuilder
		{
		public:
			PushdownAutomatonBuilder& State(Dia::Core::StringCRC stateId);
			PushdownAutomatonBuilder& InitialState(Dia::Core::StringCRC stateId);
			PushdownAutomatonBuilder& OnEnter(void(*action)(void*));
			PushdownAutomatonBuilder& OnExit(void(*action)(void*));
			PushdownAutomatonBuilder& OnUpdate(void(*action)(void*, float));
			PushdownAutomatonBuilder& OnPause(void(*action)(void*));
			PushdownAutomatonBuilder& OnResume(void(*action)(void*));

			PushdownAutomatonDefinition Build() const;

		private:
			PushdownAutomatonDefinition mDefinition;
			int mCurrentStateIndex = -1;
		};
	}
}
