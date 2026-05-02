#include "DiaStateMachine/HierarchicalStateMachineBuilder.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace StateMachine
	{
		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::State(
			Dia::Core::StringCRC stateId)
		{
			HierarchicalStateDef state;
			state.id = stateId;
			mDefinition.mStates.Add(state);
			mCurrentStateIndex = static_cast<int>(mDefinition.mStates.Size()) - 1;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::ChildState(
			Dia::Core::StringCRC stateId, Dia::Core::StringCRC parentId)
		{
			HierarchicalStateDef state;
			state.id = stateId;
			state.parentId = parentId;
			mDefinition.mStates.Add(state);
			mCurrentStateIndex = static_cast<int>(mDefinition.mStates.Size()) - 1;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::InitialState(
			Dia::Core::StringCRC stateId)
		{
			mDefinition.mInitialStateId = stateId;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::InitialChild(
			Dia::Core::StringCRC childId)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].initialChildId = childId;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::EnableHistory()
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].hasHistory = true;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::OnEnter(
			void(*action)(void*))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onEnter = action;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::OnExit(
			void(*action)(void*))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onExit = action;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::OnUpdate(
			void(*action)(void*, float))
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");
			mDefinition.mStates[mCurrentStateIndex].onUpdate = action;
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::Transition(
			Dia::Core::StringCRC targetStateId, Dia::Core::StringCRC triggerId)
		{
			DIA_ASSERT(mCurrentStateIndex >= 0, "No current state");

			HierarchicalTransitionDef transition;
			transition.sourceStateId = mDefinition.mStates[mCurrentStateIndex].id;
			transition.targetStateId = targetStateId;
			transition.triggerId = triggerId;
			mDefinition.mTransitions.Add(transition);
			return *this;
		}

		HierarchicalStateMachineBuilder& HierarchicalStateMachineBuilder::Guard(
			bool(*predicate)(const void*), Dia::Core::StringCRC guardName)
		{
			DIA_ASSERT(mDefinition.mTransitions.Size() > 0, "No transition");
			unsigned int lastIdx = mDefinition.mTransitions.Size() - 1;
			mDefinition.mTransitions[lastIdx].guard = predicate;
			mDefinition.mTransitions[lastIdx].guardName = guardName;
			return *this;
		}

		HierarchicalStateMachineDefinition HierarchicalStateMachineBuilder::Build() const
		{
			Dia::Core::Containers::DynamicArrayC<const char*, 16> errors;
			bool valid = mDefinition.Validate(errors);
			DIA_ASSERT(valid, "HierarchicalStateMachineDefinition validation failed");

			HierarchicalStateMachineDefinition result = mDefinition.Clone();
			result.mIsValid = valid;
			return result;
		}
	}
}
