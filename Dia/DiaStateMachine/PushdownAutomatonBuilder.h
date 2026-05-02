#pragma once

#include "DiaStateMachine/PushdownAutomatonDefinition.h"
#include "DiaStateMachine/StateMachineMetadata.h"

namespace Dia
{
	namespace StateMachine
	{
		class PushdownAutomatonBuilder
		{
		public:
			PushdownAutomatonBuilder& State(Dia::Core::StringCRC stateId);
			PushdownAutomatonBuilder& InitialState(Dia::Core::StringCRC stateId);
			PushdownAutomatonBuilder& OnEnter(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			PushdownAutomatonBuilder& OnExit(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			PushdownAutomatonBuilder& OnUpdate(void(*action)(void*, float),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			PushdownAutomatonBuilder& OnPause(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			PushdownAutomatonBuilder& OnResume(void(*action)(void*),
				Dia::Core::StringCRC name = Dia::Core::StringCRC());
			PushdownAutomatonBuilder& StateMetadata(Dia::Core::StringCRC key, const MetadataValue& value);
			PushdownAutomatonBuilder& MachineMetadata(Dia::Core::StringCRC key, const MetadataValue& value);

			PushdownAutomatonDefinition Build() const;

		private:
			PushdownAutomatonDefinition mDefinition;
			int mCurrentStateIndex = -1;
		};
	}
}
