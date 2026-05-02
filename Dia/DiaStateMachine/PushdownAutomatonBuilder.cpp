#include "DiaStateMachine/PushdownAutomatonBuilder.h"
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

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnEnter(void(*action)(void*))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onEnter = action;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnExit(void(*action)(void*))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onExit = action;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnUpdate(void(*action)(void*, float))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onUpdate = action;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnPause(void(*action)(void*))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onPause = action;
			return *this;
		}

		PushdownAutomatonBuilder& PushdownAutomatonBuilder::OnResume(void(*action)(void*))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onResume = action;
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
