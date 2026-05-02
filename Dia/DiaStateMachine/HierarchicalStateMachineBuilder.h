#pragma once

#include "DiaStateMachine/HierarchicalStateMachineDefinition.h"

namespace Dia
{
	namespace StateMachine
	{
		class HierarchicalStateMachineBuilder
		{
		public:
			HierarchicalStateMachineBuilder& State(Dia::Core::StringCRC stateId);
			HierarchicalStateMachineBuilder& ChildState(Dia::Core::StringCRC stateId,
				Dia::Core::StringCRC parentId);
			HierarchicalStateMachineBuilder& InitialState(Dia::Core::StringCRC stateId);
			HierarchicalStateMachineBuilder& InitialChild(Dia::Core::StringCRC childId);
			HierarchicalStateMachineBuilder& EnableHistory();
			HierarchicalStateMachineBuilder& OnEnter(void(*action)(void*));
			HierarchicalStateMachineBuilder& OnExit(void(*action)(void*));
			HierarchicalStateMachineBuilder& OnUpdate(void(*action)(void*, float));
			HierarchicalStateMachineBuilder& Transition(Dia::Core::StringCRC targetStateId,
				Dia::Core::StringCRC triggerId);
			HierarchicalStateMachineBuilder& Guard(bool(*predicate)(const void*),
				Dia::Core::StringCRC guardName = Dia::Core::StringCRC());

			HierarchicalStateMachineDefinition Build() const;

		private:
			HierarchicalStateMachineDefinition mDefinition;
			int mCurrentStateIndex = -1;
		};
	}
}
