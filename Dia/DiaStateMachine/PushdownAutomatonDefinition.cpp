#include "DiaStateMachine/PushdownAutomatonDefinition.h"

namespace Dia
{
	namespace StateMachine
	{
		PushdownAutomatonDefinition::PushdownAutomatonDefinition()
			: mIsValid(false)
		{}

		PushdownAutomatonDefinition::PushdownAutomatonDefinition(
			PushdownAutomatonDefinition&& other)
			: mStates(other.mStates)
			, mInitialStateId(other.mInitialStateId)
			, mIsValid(other.mIsValid)
		{
			other.mStates.RemoveAll();
			other.mInitialStateId = Dia::Core::StringCRC();
			other.mIsValid = false;
		}

		PushdownAutomatonDefinition& PushdownAutomatonDefinition::operator=(
			PushdownAutomatonDefinition&& other)
		{
			if (this != &other)
			{
				mStates = other.mStates;
				mInitialStateId = other.mInitialStateId;
				mIsValid = other.mIsValid;

				other.mStates.RemoveAll();
				other.mInitialStateId = Dia::Core::StringCRC();
				other.mIsValid = false;
			}
			return *this;
		}

		PushdownAutomatonDefinition PushdownAutomatonDefinition::Clone() const
		{
			PushdownAutomatonDefinition clone;
			clone.mStates = mStates;
			clone.mInitialStateId = mInitialStateId;
			clone.mIsValid = mIsValid;
			return clone;
		}

		const Dia::Core::Containers::DynamicArrayC<PushdownStateDef, PushdownAutomatonDefinition::kMaxStates>&
			PushdownAutomatonDefinition::GetStates() const
		{
			return mStates;
		}

		Dia::Core::StringCRC PushdownAutomatonDefinition::GetInitialStateId() const
		{
			return mInitialStateId;
		}

		bool PushdownAutomatonDefinition::IsValid() const
		{
			return mIsValid;
		}

		bool PushdownAutomatonDefinition::Validate(
			Dia::Core::Containers::DynamicArrayC<const char*, 16>& outErrors) const
		{
			outErrors.RemoveAll();

			if (mInitialStateId == Dia::Core::StringCRC())
			{
				outErrors.Add("Initial state not set");
			}

			bool initialFound = false;
			for (unsigned int i = 0; i < mStates.Size(); ++i)
			{
				if (mStates[i].id == mInitialStateId)
				{
					initialFound = true;
				}

				for (unsigned int j = i + 1; j < mStates.Size(); ++j)
				{
					if (mStates[i].id == mStates[j].id)
					{
						outErrors.Add("Duplicate state ID found");
					}
				}
			}

			if (mInitialStateId != Dia::Core::StringCRC() && !initialFound)
			{
				outErrors.Add("Initial state not found in state list");
			}

			return outErrors.IsEmpty();
		}
	}
}
