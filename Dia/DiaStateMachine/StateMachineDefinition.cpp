#include "DiaStateMachine/StateMachineDefinition.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace StateMachine
	{
		const Dia::Core::StringCRC kAnyState("*");

		StateMachineDefinition::StateMachineDefinition()
			: mIsValid(false)
		{}

		StateMachineDefinition::StateMachineDefinition(StateMachineDefinition&& other)
			: mStates(other.mStates)
			, mTransitions(other.mTransitions)
			, mInitialStateId(other.mInitialStateId)
			, mMetadata(other.mMetadata)
			, mStateMetadata(other.mStateMetadata)
			, mIsValid(other.mIsValid)
		{
			other.mStates.RemoveAll();
			other.mTransitions.RemoveAll();
			other.mInitialStateId = Dia::Core::StringCRC();
			other.mMetadata.RemoveAll();
			other.mStateMetadata.RemoveAll();
			other.mIsValid = false;
		}

		StateMachineDefinition& StateMachineDefinition::operator=(StateMachineDefinition&& other)
		{
			if (this != &other)
			{
				mStates = other.mStates;
				mTransitions = other.mTransitions;
				mInitialStateId = other.mInitialStateId;
				mMetadata = other.mMetadata;
				mStateMetadata = other.mStateMetadata;
				mIsValid = other.mIsValid;

				other.mStates.RemoveAll();
				other.mTransitions.RemoveAll();
				other.mInitialStateId = Dia::Core::StringCRC();
				other.mMetadata.RemoveAll();
				other.mStateMetadata.RemoveAll();
				other.mIsValid = false;
			}
			return *this;
		}

		StateMachineDefinition StateMachineDefinition::Clone() const
		{
			StateMachineDefinition clone;
			clone.mStates = mStates;
			clone.mTransitions = mTransitions;
			clone.mInitialStateId = mInitialStateId;
			clone.mMetadata = mMetadata;
			clone.mStateMetadata = mStateMetadata;
			clone.mIsValid = mIsValid;
			return clone;
		}

		Dia::Core::Containers::DynamicArrayC<StateDef, StateMachineDefinition::kMaxStates>&
			StateMachineDefinition::GetStates()
		{
			return mStates;
		}

		const Dia::Core::Containers::DynamicArrayC<StateDef, StateMachineDefinition::kMaxStates>&
			StateMachineDefinition::GetStates() const
		{
			return mStates;
		}

		Dia::Core::Containers::DynamicArrayC<TransitionDef, StateMachineDefinition::kMaxTransitions>&
			StateMachineDefinition::GetTransitions()
		{
			return mTransitions;
		}

		const Dia::Core::Containers::DynamicArrayC<TransitionDef, StateMachineDefinition::kMaxTransitions>&
			StateMachineDefinition::GetTransitions() const
		{
			return mTransitions;
		}

		Dia::Core::StringCRC StateMachineDefinition::GetInitialStateId() const
		{
			return mInitialStateId;
		}

		void StateMachineDefinition::SetInitialStateId(Dia::Core::StringCRC id)
		{
			mInitialStateId = id;
		}

		bool StateMachineDefinition::IsValid() const
		{
			return mIsValid;
		}

		void StateMachineDefinition::MarkValid()
		{
			mIsValid = true;
		}

		MetadataArray& StateMachineDefinition::GetMetadata()
		{
			return mMetadata;
		}

		const MetadataArray& StateMachineDefinition::GetMetadata() const
		{
			return mMetadata;
		}

		MetadataArray& StateMachineDefinition::GetStateMetadata(unsigned int stateIndex)
		{
			DIA_ASSERT(stateIndex < mStateMetadata.Size(), "GetStateMetadata: index out of range");
			return mStateMetadata[stateIndex];
		}

		const MetadataArray& StateMachineDefinition::GetStateMetadata(unsigned int stateIndex) const
		{
			DIA_ASSERT(stateIndex < mStateMetadata.Size(), "GetStateMetadata: index out of range");
			return mStateMetadata[stateIndex];
		}

		bool StateMachineDefinition::Validate(
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

			for (unsigned int i = 0; i < mTransitions.Size(); ++i)
			{
				const TransitionDef& t = mTransitions[i];

				if (!(t.sourceStateId == kAnyState))
				{
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
