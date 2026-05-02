#include "DiaStateMachine/HierarchicalStateMachineDefinition.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace StateMachine
	{
		HierarchicalStateMachineDefinition::HierarchicalStateMachineDefinition()
			: mIsValid(false)
		{}

		HierarchicalStateMachineDefinition::HierarchicalStateMachineDefinition(
			HierarchicalStateMachineDefinition&& other)
			: mStates(other.mStates)
			, mTransitions(other.mTransitions)
			, mInitialStateId(other.mInitialStateId)
			, mIsValid(other.mIsValid)
		{
			other.mStates.RemoveAll();
			other.mTransitions.RemoveAll();
			other.mInitialStateId = Dia::Core::StringCRC();
			other.mIsValid = false;
		}

		HierarchicalStateMachineDefinition& HierarchicalStateMachineDefinition::operator=(
			HierarchicalStateMachineDefinition&& other)
		{
			if (this != &other)
			{
				mStates = other.mStates;
				mTransitions = other.mTransitions;
				mInitialStateId = other.mInitialStateId;
				mIsValid = other.mIsValid;

				other.mStates.RemoveAll();
				other.mTransitions.RemoveAll();
				other.mInitialStateId = Dia::Core::StringCRC();
				other.mIsValid = false;
			}
			return *this;
		}

		HierarchicalStateMachineDefinition HierarchicalStateMachineDefinition::Clone() const
		{
			HierarchicalStateMachineDefinition clone;
			clone.mStates = mStates;
			clone.mTransitions = mTransitions;
			clone.mInitialStateId = mInitialStateId;
			clone.mIsValid = mIsValid;
			return clone;
		}

		const Dia::Core::Containers::DynamicArrayC<HierarchicalStateDef, HierarchicalStateMachineDefinition::kMaxStates>&
			HierarchicalStateMachineDefinition::GetStates() const
		{
			return mStates;
		}

		const Dia::Core::Containers::DynamicArrayC<HierarchicalTransitionDef, HierarchicalStateMachineDefinition::kMaxTransitions>&
			HierarchicalStateMachineDefinition::GetTransitions() const
		{
			return mTransitions;
		}

		Dia::Core::StringCRC HierarchicalStateMachineDefinition::GetInitialStateId() const
		{
			return mInitialStateId;
		}

		bool HierarchicalStateMachineDefinition::IsValid() const
		{
			return mIsValid;
		}

		bool HierarchicalStateMachineDefinition::Validate(
			Dia::Core::Containers::DynamicArrayC<const char*, 16>& outErrors) const
		{
			outErrors.RemoveAll();

			if (mInitialStateId == Dia::Core::StringCRC())
			{
				outErrors.Add("Initial state not set");
			}

			bool initialStateFound = false;
			for (unsigned int i = 0; i < mStates.Size(); ++i)
			{
				if (mStates[i].id == mInitialStateId)
				{
					initialStateFound = true;
				}

				for (unsigned int j = i + 1; j < mStates.Size(); ++j)
				{
					if (mStates[i].id == mStates[j].id)
					{
						outErrors.Add("Duplicate state ID found");
					}
				}
			}

			if (mInitialStateId != Dia::Core::StringCRC() && !initialStateFound)
			{
				outErrors.Add("Initial state not found in state list");
			}

			for (unsigned int i = 0; i < mStates.Size(); ++i)
			{
				const HierarchicalStateDef& s = mStates[i];

				if (s.parentId != Dia::Core::StringCRC())
				{
					bool parentFound = false;
					for (unsigned int j = 0; j < mStates.Size(); ++j)
					{
						if (mStates[j].id == s.parentId)
						{
							parentFound = true;
							break;
						}
					}
					if (!parentFound)
					{
						outErrors.Add("State parent not found");
					}
				}

				if (s.initialChildId != Dia::Core::StringCRC())
				{
					bool childFound = false;
					for (unsigned int j = 0; j < mStates.Size(); ++j)
					{
						if (mStates[j].id == s.initialChildId && mStates[j].parentId == s.id)
						{
							childFound = true;
							break;
						}
					}
					if (!childFound)
					{
						outErrors.Add("State initial child not found or not a child");
					}
				}
			}

			for (unsigned int i = 0; i < mTransitions.Size(); ++i)
			{
				const HierarchicalTransitionDef& t = mTransitions[i];

				bool sourceFound = false;
				for (unsigned int j = 0; j < mStates.Size(); ++j)
				{
					if (mStates[j].id == t.sourceStateId)
					{
						sourceFound = true;
						break;
					}
				}
				if (!sourceFound)
				{
					outErrors.Add("Transition source state not found");
				}

				bool targetFound = false;
				for (unsigned int j = 0; j < mStates.Size(); ++j)
				{
					if (mStates[j].id == t.targetStateId)
					{
						targetFound = true;
						break;
					}
				}
				if (!targetFound)
				{
					outErrors.Add("Transition target state not found");
				}
			}

			return outErrors.IsEmpty();
		}
	}
}
