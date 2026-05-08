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
			HierarchicalStateMachineBuilder& OnEnter(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			HierarchicalStateMachineBuilder& OnExit(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			HierarchicalStateMachineBuilder& OnUpdate(void(*action)(void*, float),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			HierarchicalStateMachineBuilder& Transition(Dia::Core::StringCRC targetStateId,
				Dia::Core::StringCRC triggerId);
			HierarchicalStateMachineBuilder& Guard(bool(*predicate)(const void*),
				Dia::Core::StringCRC guardName = Dia::Core::StringCRC());
			HierarchicalStateMachineBuilder& StateMetadata(Dia::Core::StringCRC key, const MetadataValue& value);
			HierarchicalStateMachineBuilder& MachineMetadata(Dia::Core::StringCRC key, const MetadataValue& value);

			HierarchicalStateMachineDefinition Build() const;

		private:
			HierarchicalStateMachineDefinition mDefinition;
			int mCurrentStateIndex = -1;
		};
	}
}
