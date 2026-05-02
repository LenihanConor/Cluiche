#pragma once

#include "DiaStateMachine/StateMachineDefinition.h"
#include "DiaStateMachine/StateMachineMetadata.h"

namespace Dia
{
	namespace StateMachine
	{
		class StateMachineBuilder
		{
		public:
			StateMachineBuilder& State(Dia::Core::StringCRC stateId);
			StateMachineBuilder& InitialState(Dia::Core::StringCRC stateId);
			StateMachineBuilder& OnEnter(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			StateMachineBuilder& OnExit(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			StateMachineBuilder& OnUpdate(void(*action)(void*, float),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			StateMachineBuilder& Transition(Dia::Core::StringCRC targetStateId,
				Dia::Core::StringCRC triggerId);
			StateMachineBuilder& Guard(bool(*predicate)(const void*),
				Dia::Core::StringCRC guardName = Dia::Core::StringCRC());
			StateMachineBuilder& StateMetadata(Dia::Core::StringCRC key, const MetadataValue& value);
			StateMachineBuilder& MachineMetadata(Dia::Core::StringCRC key, const MetadataValue& value);

			StateMachineDefinition Build() const;

		private:
			StateMachineDefinition mDefinition;
			int mCurrentStateIndex = -1;
		};
	}
}
