#pragma once

#include "DiaStateMachine/IStateMachineInspectable.h"
#include "DiaStateMachine/PushdownAutomatonDefinition.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace StateMachine
	{
		template<typename TContext>
		class PushdownAutomaton : public IStateMachineInspectable
		{
		public:
			static const int kMaxStackDepth = 16;
			static const int kHistorySize = 32;

			explicit PushdownAutomaton(Dia::Core::StringCRC machineId,
				PushdownAutomatonDefinition&& definition,
				TContext& context);

			Dia::Core::StringCRC GetCurrentStateId() const override;
			bool IsInState(Dia::Core::StringCRC stateId) const;

			bool Push(Dia::Core::StringCRC stateId);
			bool Pop();
			void Update(float deltaTime);

			int GetStackDepth() const;

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

			PushdownAutomatonDefinition mDefinition;
			TContext& mContext;
			Dia::Core::StringCRC mMachineId;

			Dia::Core::Containers::DynamicArrayC<int, kMaxStackDepth> mStack;

			float mCurrentTime = 0.0f;

			Dia::Core::Containers::DynamicArrayC<TransitionRecord, kHistorySize> mHistory;
			ITransitionListener* mListener = nullptr;
		};

		// ---- Implementation ----

		template<typename TContext>
		PushdownAutomaton<TContext>::PushdownAutomaton(
			Dia::Core::StringCRC machineId,
			PushdownAutomatonDefinition&& definition,
			TContext& context)
			: mDefinition(static_cast<PushdownAutomatonDefinition&&>(definition))
			, mContext(context)
			, mMachineId(machineId)
		{
			DIA_ASSERT(mDefinition.IsValid(), "Definition must be valid");

			int initialIndex = FindStateIndex(mDefinition.GetInitialStateId());
			DIA_ASSERT(initialIndex >= 0, "Initial state not found");

			mStack.Add(initialIndex);

			const PushdownStateDef& state = mDefinition.GetStates()[initialIndex];
			if (state.onEnter)
			{
				state.onEnter(static_cast<void*>(&mContext));
			}
		}

		template<typename TContext>
		Dia::Core::StringCRC PushdownAutomaton<TContext>::GetCurrentStateId() const
		{
			if (mStack.Size() > 0)
			{
				int topIndex = mStack[mStack.Size() - 1];
				return mDefinition.GetStates()[topIndex].id;
			}
			return Dia::Core::StringCRC();
		}

		template<typename TContext>
		bool PushdownAutomaton<TContext>::IsInState(Dia::Core::StringCRC stateId) const
		{
			return GetCurrentStateId() == stateId;
		}

		template<typename TContext>
		bool PushdownAutomaton<TContext>::Push(Dia::Core::StringCRC stateId)
		{
			DIA_ASSERT(static_cast<int>(mStack.Size()) < kMaxStackDepth,
				"Push exceeds max stack depth");

			int newIndex = FindStateIndex(stateId);
			if (newIndex < 0)
			{
				return false;
			}

			Dia::Core::StringCRC sourceId = GetCurrentStateId();

			int currentTop = mStack[mStack.Size() - 1];
			const PushdownStateDef& currentState = mDefinition.GetStates()[currentTop];
			if (currentState.onPause)
			{
				currentState.onPause(static_cast<void*>(&mContext));
			}

			mStack.Add(newIndex);

			const PushdownStateDef& newState = mDefinition.GetStates()[newIndex];
			if (newState.onEnter)
			{
				newState.onEnter(static_cast<void*>(&mContext));
			}

			TransitionRecord record;
			record.sourceStateId = sourceId;
			record.targetStateId = stateId;
			record.triggerId = Dia::Core::StringCRC("Push");
			record.timestamp = mCurrentTime;
			mHistory.Add(record);

			if (mListener)
			{
				TransitionEvent event;
				event.sourceStateId = sourceId;
				event.targetStateId = stateId;
				event.triggerId = Dia::Core::StringCRC("Push");
				event.timestamp = mCurrentTime;
				mListener->OnTransition(event);
			}

			return true;
		}

		template<typename TContext>
		bool PushdownAutomaton<TContext>::Pop()
		{
			if (mStack.Size() <= 1)
			{
				return false;
			}

			int topIndex = mStack[mStack.Size() - 1];
			Dia::Core::StringCRC sourceId = mDefinition.GetStates()[topIndex].id;

			const PushdownStateDef& exitingState = mDefinition.GetStates()[topIndex];
			if (exitingState.onExit)
			{
				exitingState.onExit(static_cast<void*>(&mContext));
			}

			mStack.RemoveAt(mStack.Size() - 1);

			int newTop = mStack[mStack.Size() - 1];
			const PushdownStateDef& resumingState = mDefinition.GetStates()[newTop];
			Dia::Core::StringCRC targetId = resumingState.id;

			if (resumingState.onResume)
			{
				resumingState.onResume(static_cast<void*>(&mContext));
			}

			TransitionRecord record;
			record.sourceStateId = sourceId;
			record.targetStateId = targetId;
			record.triggerId = Dia::Core::StringCRC("Pop");
			record.timestamp = mCurrentTime;
			mHistory.Add(record);

			if (mListener)
			{
				TransitionEvent event;
				event.sourceStateId = sourceId;
				event.targetStateId = targetId;
				event.triggerId = Dia::Core::StringCRC("Pop");
				event.timestamp = mCurrentTime;
				mListener->OnTransition(event);
			}

			return true;
		}

		template<typename TContext>
		void PushdownAutomaton<TContext>::Update(float deltaTime)
		{
			mCurrentTime += deltaTime;

			if (mStack.Size() > 0)
			{
				int topIndex = mStack[mStack.Size() - 1];
				const PushdownStateDef& state = mDefinition.GetStates()[topIndex];
				if (state.onUpdate)
				{
					state.onUpdate(static_cast<void*>(&mContext), deltaTime);
				}
			}
		}

		template<typename TContext>
		int PushdownAutomaton<TContext>::GetStackDepth() const
		{
			return static_cast<int>(mStack.Size());
		}

		template<typename TContext>
		Dia::Core::StringCRC PushdownAutomaton<TContext>::GetMachineId() const
		{
			return mMachineId;
		}

		template<typename TContext>
		void PushdownAutomaton<TContext>::GetAllStates(
			Dia::Core::Containers::DynamicArrayC<StateInfo, 64>& outStates) const
		{
			outStates.RemoveAll();
			const auto& states = mDefinition.GetStates();
			Dia::Core::StringCRC currentId = GetCurrentStateId();
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				StateInfo info;
				info.id = states[i].id;
				info.isActive = (states[i].id == currentId);
				info.isLeaf = true;
				info.parentId = Dia::Core::StringCRC();
				outStates.Add(info);
			}
		}

		template<typename TContext>
		void PushdownAutomaton<TContext>::GetAllTransitions(
			Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64>& outTransitions) const
		{
			outTransitions.RemoveAll();
		}

		template<typename TContext>
		void PushdownAutomaton<TContext>::GetTransitionHistory(
			Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32>& outHistory) const
		{
			outHistory.RemoveAll();
			for (unsigned int i = 0; i < mHistory.Size(); ++i)
			{
				outHistory.Add(mHistory[i]);
			}
		}

		template<typename TContext>
		void PushdownAutomaton<TContext>::SetTransitionListener(ITransitionListener* listener)
		{
			mListener = listener;
		}

		template<typename TContext>
		int PushdownAutomaton<TContext>::FindStateIndex(Dia::Core::StringCRC stateId) const
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
	}
}
