#include "DiaStateMachine/StateMachineBuilder.h"
#include "DiaStateMachine/StateMachineMetadata.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace StateMachine
	{
		StateMachineBuilder& StateMachineBuilder::State(Dia::Core::StringCRC stateId)
		{
			StateDef state;
			state.id = stateId;
			mDefinition.mStates.Add(state);
			mCurrentStateIndex = static_cast<int>(mDefinition.mStates.Size()) - 1;
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::InitialState(Dia::Core::StringCRC stateId)
		{
			mDefinition.mInitialStateId = stateId;
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::OnEnter(void(*action)(void*), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state — call State() first");
			mDefinition.mStates[mCurrentStateIndex].onEnter = action;
			mDefinition.mStates[mCurrentStateIndex].onEnterName = name;
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::OnExit(void(*action)(void*), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state — call State() first");
			mDefinition.mStates[mCurrentStateIndex].onExit = action;
			mDefinition.mStates[mCurrentStateIndex].onExitName = name;
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::OnUpdate(void(*action)(void*, float), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state — call State() first");
			mDefinition.mStates[mCurrentStateIndex].onUpdate = action;
			mDefinition.mStates[mCurrentStateIndex].onUpdateName = name;
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::StateMetadata(Dia::Core::StringCRC key, const MetadataValue& value)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state — call State() first");
			Dia::StateMachine::SetMetadata(mDefinition.mStates[mCurrentStateIndex].metadata, key, value);
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::MachineMetadata(Dia::Core::StringCRC key, const MetadataValue& value)
		{
			Dia::StateMachine::SetMetadata(mDefinition.mMetadata, key, value);
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::Transition(
			Dia::Core::StringCRC targetStateId,
			Dia::Core::StringCRC triggerId)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state — call State() first");

			TransitionDef transition;
			transition.sourceStateId = mDefinition.mStates[mCurrentStateIndex].id;
			transition.targetStateId = targetStateId;
			transition.triggerId = triggerId;
			mDefinition.mTransitions.Add(transition);
			return *this;
		}

		StateMachineBuilder& StateMachineBuilder::Guard(
			bool(*predicate)(const void*),
			Dia::Core::StringCRC guardName)
		{
			DIA_ASSERT(mDefinition.mTransitions.Size() > 0, "No transition — call Transition() first");
			unsigned int lastIdx = mDefinition.mTransitions.Size() - 1;
			mDefinition.mTransitions[lastIdx].guard = predicate;
			mDefinition.mTransitions[lastIdx].guardName = guardName;
			return *this;
		}

		StateMachineDefinition StateMachineBuilder::Build() const
		{
			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			bool valid = mDefinition.Validate(errors);
			DIA_ASSERT(valid, "StateMachineDefinition validation failed");

			StateMachineDefinition result = mDefinition.Clone();
			result.mIsValid = valid;
			return result;
		}
	}
}
