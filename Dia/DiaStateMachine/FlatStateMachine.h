#pragma once

#include "DiaStateMachine/IStateMachineInspectable.h"
#include "DiaStateMachine/StateMachineDefinition.h"
#include "DiaCore/Core/Assert.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace StateMachine
	{
		template<typename TContext>
		class FlatStateMachine : public IStateMachineInspectable
		{
		public:
			using ActionFn = void(*)(TContext&);
			using GuardFn = bool(*)(const TContext&);
			using UpdateFn = void(*)(TContext&, float deltaTime);

			static const int kMaxTransitionsPerFrame = 8;
			static const int kHistorySize = 32;

			explicit FlatStateMachine(Dia::Core::StringCRC machineId,
				StateMachineDefinition&& definition,
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
			void EnterState(int stateIndex);
			void ExitState(int stateIndex);
			void RecordTransition(Dia::Core::StringCRC source, Dia::Core::StringCRC target,
				Dia::Core::StringCRC trigger, float timestamp);
			void NotifyTransition(const TransitionEvent& event);
			void NotifyTransitionFailed(Dia::Core::StringCRC triggerId);

			StateMachineDefinition mDefinition;
			TContext& mContext;
			Dia::Core::StringCRC mMachineId;
			int mCurrentStateIndex = -1;
			int mTransitionsThisFrame = 0;
			float mCurrentTime = 0.0f;
			float mStateEntryTime = 0.0f;
			Dia::Core::Containers::DynamicArrayC<TransitionRecord, kHistorySize> mHistory;
			ITransitionListener* mListener = nullptr;
		};

		// ---- Implementation ----

		template<typename TContext>
		FlatStateMachine<TContext>::FlatStateMachine(
			Dia::Core::StringCRC machineId,
			StateMachineDefinition&& definition,
			TContext& context)
			: mDefinition(static_cast<StateMachineDefinition&&>(definition))
			, mContext(context)
			, mMachineId(machineId)
		{
			DIA_ASSERT(mDefinition.IsValid(), "Definition must be valid");

			mCurrentStateIndex = FindStateIndex(mDefinition.GetInitialStateId());
			DIA_ASSERT(mCurrentStateIndex >= 0, "Initial state not found");

			EnterState(mCurrentStateIndex);
		}

		template<typename TContext>
		Dia::Core::StringCRC FlatStateMachine<TContext>::GetCurrentStateId() const
		{
			if (mCurrentStateIndex >= 0)
			{
				return mDefinition.GetStates()[mCurrentStateIndex].id;
			}
			return Dia::Core::StringCRC();
		}

		template<typename TContext>
		bool FlatStateMachine<TContext>::IsInState(Dia::Core::StringCRC stateId) const
		{
			return GetCurrentStateId() == stateId;
		}

		template<typename TContext>
		bool FlatStateMachine<TContext>::Fire(Dia::Core::StringCRC triggerId)
		{
			if (mTransitionsThisFrame >= kMaxTransitionsPerFrame)
			{
				DIA_ASSERT(false, "Max transitions per frame exceeded");
				return false;
			}

			const auto& transitions = mDefinition.GetTransitions();
			Dia::Core::StringCRC currentId = GetCurrentStateId();

			Dia::Core::Containers::DynamicArrayC<GuardEvaluation, 16> guardEvals;

			// State-specific transitions first
			for (unsigned int i = 0; i < transitions.Size(); ++i)
			{
				const TransitionDef& t = transitions[i];
				if (t.sourceStateId == currentId && t.triggerId == triggerId)
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

					float residenceTime = mCurrentTime - mStateEntryTime;

					ExitState(mCurrentStateIndex);

					TransitionRecord record;
					record.sourceStateId = currentId;
					record.targetStateId = t.targetStateId;
					record.triggerId = triggerId;
					record.timestamp = mCurrentTime;
					mHistory.Add(record);

					mCurrentStateIndex = targetIndex;
					mStateEntryTime = mCurrentTime;
					++mTransitionsThisFrame;

					EnterState(mCurrentStateIndex);

					if (mListener)
					{
						TransitionEvent event;
						event.sourceStateId = currentId;
						event.targetStateId = t.targetStateId;
						event.triggerId = triggerId;
						event.timestamp = mCurrentTime;
						event.stateResidenceTime = residenceTime;
						event.guardResults = guardEvals.Size() > 0 ? &guardEvals[0] : nullptr;
						event.guardCount = static_cast<int>(guardEvals.Size());
						NotifyTransition(event);
					}

					return true;
				}
			}

			// Wildcard transitions (kAnyState)
			for (unsigned int i = 0; i < transitions.Size(); ++i)
			{
				const TransitionDef& t = transitions[i];
				if (t.sourceStateId == kAnyState && t.triggerId == triggerId)
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

					float residenceTime = mCurrentTime - mStateEntryTime;

					ExitState(mCurrentStateIndex);

					TransitionRecord record;
					record.sourceStateId = currentId;
					record.targetStateId = t.targetStateId;
					record.triggerId = triggerId;
					record.timestamp = mCurrentTime;
					mHistory.Add(record);

					mCurrentStateIndex = targetIndex;
					mStateEntryTime = mCurrentTime;
					++mTransitionsThisFrame;

					EnterState(mCurrentStateIndex);

					if (mListener)
					{
						TransitionEvent event;
						event.sourceStateId = currentId;
						event.targetStateId = t.targetStateId;
						event.triggerId = triggerId;
						event.timestamp = mCurrentTime;
						event.stateResidenceTime = residenceTime;
						event.guardResults = guardEvals.Size() > 0 ? &guardEvals[0] : nullptr;
						event.guardCount = static_cast<int>(guardEvals.Size());
						NotifyTransition(event);
					}

					return true;
				}
			}

			if (mListener)
			{
				NotifyTransitionFailed(triggerId);
			}

			return false;
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::Update(float deltaTime)
		{
			mTransitionsThisFrame = 0;
			mCurrentTime += deltaTime;

			if (mCurrentStateIndex >= 0)
			{
				const StateDef& state = mDefinition.GetStates()[mCurrentStateIndex];
				if (state.onUpdate)
				{
					state.onUpdate(static_cast<void*>(&mContext), deltaTime);
				}
			}
		}

		template<typename TContext>
		Dia::Core::StringCRC FlatStateMachine<TContext>::GetMachineId() const
		{
			return mMachineId;
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::GetAllStates(
			Dia::Core::Containers::DynamicArrayC<StateInfo, 64>& outStates) const
		{
			outStates.RemoveAll();
			const auto& states = mDefinition.GetStates();
			for (unsigned int i = 0; i < states.Size(); ++i)
			{
				StateInfo info;
				info.id = states[i].id;
				info.isActive = (static_cast<int>(i) == mCurrentStateIndex);
				info.isLeaf = true;
				info.parentId = Dia::Core::StringCRC();
				outStates.Add(info);
			}
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::GetAllTransitions(
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
		void FlatStateMachine<TContext>::GetTransitionHistory(
			Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32>& outHistory) const
		{
			outHistory.RemoveAll();
			for (unsigned int i = 0; i < mHistory.Size(); ++i)
			{
				outHistory.Add(mHistory[i]);
			}
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::SetTransitionListener(ITransitionListener* listener)
		{
			mListener = listener;
		}

		template<typename TContext>
		int FlatStateMachine<TContext>::FindStateIndex(Dia::Core::StringCRC stateId) const
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
		void FlatStateMachine<TContext>::EnterState(int stateIndex)
		{
			const StateDef& state = mDefinition.GetStates()[stateIndex];
			if (state.onEnter)
			{
				state.onEnter(static_cast<void*>(&mContext));
			}
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::ExitState(int stateIndex)
		{
			const StateDef& state = mDefinition.GetStates()[stateIndex];
			if (state.onExit)
			{
				state.onExit(static_cast<void*>(&mContext));
			}
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::RecordTransition(
			Dia::Core::StringCRC source, Dia::Core::StringCRC target,
			Dia::Core::StringCRC trigger, float timestamp)
		{
			TransitionRecord record;
			record.sourceStateId = source;
			record.targetStateId = target;
			record.triggerId = trigger;
			record.timestamp = timestamp;
			mHistory.Add(record);
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::NotifyTransition(const TransitionEvent& event)
		{
			if (mListener)
			{
				mListener->OnTransition(event);
			}
		}

		template<typename TContext>
		void FlatStateMachine<TContext>::NotifyTransitionFailed(Dia::Core::StringCRC triggerId)
		{
			if (mListener)
			{
				mListener->OnTransitionFailed(mMachineId, GetCurrentStateId(), triggerId);
			}
		}
	}
}
