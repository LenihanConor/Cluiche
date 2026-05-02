#pragma once

#include "DiaStateMachine/StateMachineDefinition.h"

namespace Dia
{
	namespace StateMachine
	{
		class StateMachineBuilder
		{
		public:
			StateMachineBuilder& State(Dia::Core::StringCRC stateId);
			StateMachineBuilder& InitialState(Dia::Core::StringCRC stateId);
			StateMachineBuilder& OnEnter(void(*action)(void*));
			StateMachineBuilder& OnExit(void(*action)(void*));
			StateMachineBuilder& OnUpdate(void(*action)(void*, float));
			StateMachineBuilder& Transition(Dia::Core::StringCRC targetStateId,
				Dia::Core::StringCRC triggerId);
			StateMachineBuilder& Guard(bool(*predicate)(const void*),
				Dia::Core::StringCRC guardName = Dia::Core::StringCRC());

			StateMachineDefinition Build() const;

		private:
			StateMachineDefinition mDefinition;
			int mCurrentStateIndex = -1;
		};
	}
}
