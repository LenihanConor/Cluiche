#pragma once

#include "DiaStateMachine/IStateMachineInspectable.h"
#include "DiaStateMachine/HierarchicalStateMachineDefinition.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace StateMachine
	{
		template<typename TContext>
		class HierarchicalStateMachine : public IStateMachineInspectable
		{
		public:
			static const int kMaxDepth = 16;
			static const int kMaxTransitionsPerFrame = 8;
			static const int kHistorySize = 32;

			explicit HierarchicalStateMachine(Dia::Core::StringCRC machineId,
				HierarchicalStateMachineDefinition&& definition,
				TContext& context);

			Dia::Core::StringCRC GetCurrentStateId() const override;
			bool IsInState(Dia::Core::StringCRC stateId) const;

			bool Fire(Dia::Core::StringCRC triggerId);
			void Update(float deltaTime);

			Dia::Core::StringCRC GetMachineId() const override;

			void GetAllStates(
				Dia::Core::Containers::DynamicArrayC<StateInfo, 64>& outStates) const override;
			void GetAllTransitions(
				Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64>& outTransitions) const override;
			void GetTransitionHistory(
				Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32>& outHistory) const override;

			void SetTransitionListener(ITransitionListener* listener) override;

		private:
			int FindStateIndex(Dia::Core::StringCRC stateId) const;
			bool IsLeaf(int stateIndex) const;
			bool IsAncestor(int ancestorIndex, int descendantIndex) const;
			int FindLCA(int stateA, int stateB) const;
			void GetPathToRoot(int stateIndex,
				Dia::Core::Containers::DynamicArrayC<int, kMaxDepth>& outPath) const;
			void EnterStateRecursive(int stateIndex);
			void ExitToLCA(int lcaIndex);

			HierarchicalStateMachineDefinition mDefinition;
			TContext& mContext;
			Dia::Core::StringCRC mMachineId;

			Dia::Core::Containers::DynamicArrayC<int, kMaxDepth> mActiveStatePath;

			Dia::Core::StringCRC mHistoryStates[HierarchicalStateMachineDefinition::kMaxStates];

			int mTransitionsThisFrame = 0;
			float mCurrentTime = 0.0f;
			float mStateEntryTime = 0.0f;

			Dia::Core::Containers::DynamicArrayC<TransitionRecord, kHistorySize> mHistory;
			ITransitionListener* mListener = nullptr;
		};

		// ---- Implementation ----

		template<typename TContext>
		HierarchicalStateMachine<TContext>::HierarchicalStateMachine(
			Dia::Core::StringCRC machineId,
			HierarchicalStateMachineDefinition&& definition,
			TContext& context)
			: mDefinition(static_cast<HierarchicalStateMachineDefinition&&>(definition))
			, mContext(context)
			, mMachineId(machineId)
		{
			DIA_ASSERT(mDefinition.IsValid(), "Definition must be valid");

			for (unsigned int i = 0; i < HierarchicalStateMachineDefinition::kMaxStates; ++i)
			{
				mHistoryStates[i] = Dia::Core::StringCRC();
			}

			int initialIndex = FindStateIndex(mDefinition.GetInitialStateId());
			DIA_ASSERT(initialIndex >= 0, "Initial state not found");

			EnterStateRecursive(initialIndex);
		}

		template<typename TContext>
		Dia::Core::StringCRC HierarchicalStateMachine<TContext>::GetCurrentStateId() const
		{
			if (mActiveStatePath.Size() > 0)
			{
				int leafIndex = mActiveStatePath[mActiveStatePath.Size() - 1];
				return mDefinition.GetStates()[leafIndex].id;
			}
			return Dia::Core::StringCRC();
		}

		template<typename TContext>
		bool HierarchicalStateMachine<TContext>::IsInState(Dia::Core::StringCRC stateId) const
		{
			for (unsigned int i = 0; i < mActiveStatePath.Size(); ++i)
			{
				if (mDefinition.GetStates()[mActiveStatePath[i]].id == stateId)
				{
					return true;
				}
			}
			return false;
		}

		template<typename TContext>
		bool HierarchicalStateMachine<TContext>::Fire(Dia::Core::StringCRC triggerId)
		{
			if (mTransitionsThisFrame >= kMaxTransitionsPerFrame)
			{
				DIA_ASSERT(false, "Max transitions per frame exceeded");
				return false;
			}

			const auto& transitions = mDefinition.GetTransitions();
			Dia::Core::Containers::DynamicArrayC<GuardEvaluation, 16> guardEvals;

			for (int depth = static_cast<int>(mActiveStatePath.Size()) - 1; depth >= 0; --depth)
			{
				int activeIndex = mActiveStatePath[depth];
				Dia::Core::StringCRC activeId = mDefinition.GetStates()[activeIndex].id;

				for (unsigned int i = 0; i < transitions.Size(); ++i)
				{
					const HierarchicalTransitionDef& t = transitions[i];
					if (t.sourceStateId == activeId && t.triggerId == triggerId)
					{
						if (t.guard != nullptr)
						{
							bool passed = t.guard(static_cast<const void*>(&mContext));
							GuardEvaluation eval;
							eval.guardName = t.guardName;
							eval.passed = passed;
							guardEvals.Add(eval);

							if (!passed)
							{
								continue;
							}
						}

						int targetIndex = FindStateIndex(t.targetStateId);
						DIA_ASSERT(targetIndex >= 0, "Transition target state not found");

						Dia::Core::StringCRC sourceLeafId = GetCurrentStateId();
						float residenceTime = mCurrentTime - mStateEntryTime;

						if (activeIndex == targetIndex)
						{
							ExitToLCA(activeIndex);
							mActiveStatePath.RemoveAt(mActiveStatePath.Size() - 1);
							if (mDefinition.GetStates()[activeIndex].onExit)
							{
								mDefinition.GetStates()[activeIndex].onExit(
									static_cast<void*>(&mContext));
							}
							EnterStateRecursive(targetIndex);
						}
						else
						{
							int lcaIndex = FindLCA(activeIndex, targetIndex);
							ExitToLCA(lcaIndex);
							EnterStateRecursive(targetIndex);
						}

						TransitionRecord record;
						record.sourceStateId = sourceLeafId;
						record.targetStateId = GetCurrentStateId();
						record.triggerId = triggerId;
						record.timestamp = mCurrentTime;
						mHistory.Add(record);

						mStateEntryTime = mCurrentTime;
						++mTransitionsThisFrame;

						if (mListener)
						{
							TransitionEvent event;
							event.sourceStateId = sourceLeafId;
							event.targetStateId = GetCurrentStateId();
							event.triggerId = triggerId;
							event.timestamp = mCurrentTime;
							event.stateResidenceTime = residenceTime;
							event.guardResults = guardEvals.Size() > 0 ? &guardEvals[0] : nullptr;
							event.guardCount = static_cast<int>(guardEvals.Size());
							mListener->OnTransition(event);
						}

						return true;
					}
				}
			}

			if (mListener)
			{
				mListener->OnTransitionFailed(mMachineId, GetCurrentStateId(), triggerId);
			}

			return false;
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::Update(float deltaTime)
		{
			mTransitionsThisFrame = 0;
			mCurrentTime += deltaTime;

			for (unsigned int i = 0; i < mActiveStatePath.Size(); ++i)
			{
				const HierarchicalStateDef& state = mDefinition.GetStates()[mActiveStatePath[i]];
				if (state.onUpdate)
				{
					state.onUpdate(static_cast<void*>(&mContext), deltaTime);
				}
			}
		}

		template<typename TContext>
		Dia::Core::StringCRC HierarchicalStateMachine<TContext>::GetMachineId() const
		{
			return mMachineId;
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::GetAllStates(
			Dia::Core::Containers::DynamicArrayC<StateInfo, 64>& outStates) const
		{
			outStates.RemoveAll();
			const auto& states = mDefinition.GetStates();
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				StateInfo info;
				info.id = states[i].id;
				info.parentId = states[i].parentId;
				info.isLeaf = IsLeaf(static_cast<int>(i));
				info.isActive = IsInState(states[i].id);
				outStates.Add(info);
			}
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::GetAllTransitions(
			Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64>& outTransitions) const
		{
			outTransitions.RemoveAll();
			const auto& transitions = mDefinition.GetTransitions();
			for (unsigned int i = 0; i < transitions.Size(); ++i)
			{
				TransitionInfo info;
				info.sourceStateId = transitions[i].sourceStateId;
				info.targetStateId = transitions[i].targetStateId;
				info.triggerId = transitions[i].triggerId;
				info.hasGuard = (transitions[i].guard != nullptr);
				outTransitions.Add(info);
			}
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::GetTransitionHistory(
			Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32>& outHistory) const
		{
			outHistory.RemoveAll();
			for (unsigned int i = 0; i < mHistory.Size(); ++i)
			{
				outHistory.Add(mHistory[i]);
			}
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::SetTransitionListener(ITransitionListener* listener)
		{
			mListener = listener;
		}

		template<typename TContext>
		int HierarchicalStateMachine<TContext>::FindStateIndex(Dia::Core::StringCRC stateId) const
		{
			const auto& states = mDefinition.GetStates();
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				if (states[i].id == stateId)
				{
					return static_cast<int>(i);
				}
			}
			return -1;
		}

		template<typename TContext>
		bool HierarchicalStateMachine<TContext>::IsLeaf(int stateIndex) const
		{
			const auto& states = mDefinition.GetStates();
			Dia::Core::StringCRC id = states[stateIndex].id;
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				if (states[i].parentId == id)
				{
					return false;
				}
			}
			return true;
		}

		template<typename TContext>
		bool HierarchicalStateMachine<TContext>::IsAncestor(int ancestorIndex, int descendantIndex) const
		{
			const auto& states = mDefinition.GetStates();
			int current = descendantIndex;
			while (current >= 0)
			{
				if (current == ancestorIndex)
				{
					return true;
				}
				Dia::Core::StringCRC parentId = states[current].parentId;
				if (parentId == Dia::Core::StringCRC())
				{
					break;
				}
				current = FindStateIndex(parentId);
			}
			return false;
		}

		template<typename TContext>
		int HierarchicalStateMachine<TContext>::FindLCA(int stateA, int stateB) const
		{
			Dia::Core::Containers::DynamicArrayC<int, kMaxDepth> pathA;
			GetPathToRoot(stateA, pathA);

			int current = stateB;
			while (current >= 0)
			{
				for (unsigned int i = 0; i < pathA.Size(); ++i)
				{
					if (pathA[i] == current)
					{
						return current;
					}
				}
				Dia::Core::StringCRC parentId = mDefinition.GetStates()[current].parentId;
				if (parentId == Dia::Core::StringCRC())
				{
					break;
				}
				current = FindStateIndex(parentId);
			}

			return -1;
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::GetPathToRoot(int stateIndex,
			Dia::Core::Containers::DynamicArrayC<int, kMaxDepth>& outPath) const
		{
			outPath.RemoveAll();
			int current = stateIndex;
			while (current >= 0)
			{
				outPath.Add(current);
				Dia::Core::StringCRC parentId = mDefinition.GetStates()[current].parentId;
				if (parentId == Dia::Core::StringCRC())
				{
					break;
				}
				current = FindStateIndex(parentId);
			}
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::EnterStateRecursive(int stateIndex)
		{
			const auto& states = mDefinition.GetStates();

			Dia::Core::Containers::DynamicArrayC<int, kMaxDepth> entryPath;
			int current = stateIndex;
			while (current >= 0)
			{
				bool alreadyActive = false;
				for (unsigned int i = 0; i < mActiveStatePath.Size(); ++i)
				{
					if (mActiveStatePath[i] == current)
					{
						alreadyActive = true;
						break;
					}
				}
				if (alreadyActive)
				{
					break;
				}
				entryPath.Add(current);
				Dia::Core::StringCRC parentId = states[current].parentId;
				if (parentId == Dia::Core::StringCRC())
				{
					break;
				}
				current = FindStateIndex(parentId);
			}

			for (int i = static_cast<int>(entryPath.Size()) - 1; i >= 0; --i)
			{
				int idx = entryPath[i];
				mActiveStatePath.Add(idx);
				if (states[idx].onEnter)
				{
					states[idx].onEnter(static_cast<void*>(&mContext));
				}
			}

			int leafIdx = stateIndex;
			if (!IsLeaf(leafIdx))
			{
				Dia::Core::StringCRC childToEnter;

				if (states[leafIdx].hasHistory &&
					mHistoryStates[leafIdx] != Dia::Core::StringCRC())
				{
					childToEnter = mHistoryStates[leafIdx];
				}
				else
				{
					childToEnter = states[leafIdx].initialChildId;
				}

				if (childToEnter != Dia::Core::StringCRC())
				{
					int childIndex = FindStateIndex(childToEnter);
					DIA_ASSERT(childIndex >= 0, "Child state not found");
					EnterStateRecursive(childIndex);
				}
			}
		}

		template<typename TContext>
		void HierarchicalStateMachine<TContext>::ExitToLCA(int lcaIndex)
		{
			const auto& states = mDefinition.GetStates();

			while (mActiveStatePath.Size() > 0)
			{
				int topIndex = mActiveStatePath[mActiveStatePath.Size() - 1];
				if (topIndex == lcaIndex)
				{
					break;
				}

				Dia::Core::StringCRC parentId = states[topIndex].parentId;
				if (parentId != Dia::Core::StringCRC())
				{
					int parentIndex = FindStateIndex(parentId);
					if (parentIndex >= 0)
					{
						mHistoryStates[parentIndex] = states[topIndex].id;
					}
				}

				if (states[topIndex].onExit)
				{
					states[topIndex].onExit(static_cast<void*>(&mContext));
				}
				mActiveStatePath.RemoveAt(mActiveStatePath.Size() - 1);
			}
		}
	}
}
