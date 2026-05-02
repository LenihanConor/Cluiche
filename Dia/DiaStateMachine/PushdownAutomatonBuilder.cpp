#include "DiaStateMachine/PushdownAutomatonBuilder.h"
#include "DiaStateMachine/StateMachineMetadata.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace StateMachine
	{
		PushdownAutomatonBuilder& PushdownAutomatonBuilder::State(
			Dia::Core::StringCRC stateId)
		{
			PushdownStateDef state;
			state.id = stateId;
			mDefinition.mStates.Add(state);
			mCurrentStateIndex = static_cast<int>(mDefinition.mStates.Size()) - 1;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::InitialState(
			Dia::Core::StringCRC stateId)
		{
			mDefinition.mInitialStateId = stateId;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnEnter(void(*action)(void*), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onEnter = action;
			mDefinition.mStates[mCurrentStateIndex].onEnterName = name;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnExit(void(*action)(void*), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onExit = action;
			mDefinition.mStates[mCurrentStateIndex].onExitName = name;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnUpdate(void(*action)(void*, float), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onUpdate = action;
			mDefinition.mStates[mCurrentStateIndex].onUpdateName = name;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnPause(void(*action)(void*), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onPause = action;
			mDefinition.mStates[mCurrentStateIndex].onPauseName = name;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnResume(void(*action)(void*), Dia::Core::StringCRC name)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onResume = action;
			mDefinition.mStates[mCurrentStateIndex].onResumeName = name;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::StateMetadata(
			Dia::Core::StringCRC key, const MetadataValue& value)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			Dia::StateMachine::SetMetadata(mDefinition.mStates[mCurrentStateIndex].metadata, key, value);
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::MachineMetadata(
			Dia::Core::StringCRC key, const MetadataValue& value)
		{
			Dia::StateMachine::SetMetadata(mDefinition.mMetadata, key, value);
			return *this;
		}

		PushdownAutomatonDefinition PushdownAutomatonBuilder::Build() const
		{
			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			bool valid = mDefinition.Validate(errors);
			DIA_ASSERT(valid, "PushdownAutomatonDefinition validation failed");

			PushdownAutomatonDefinition result = mDefinition.Clone();
			result.mIsValid = valid;
			return result;
		}
	}
}
