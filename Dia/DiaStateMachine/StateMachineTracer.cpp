#include "DiaStateMachine/StateMachineTracer.h"
#include "DiaLogger/DiaLog.h"

namespace Dia
{
	namespace StateMachine
	{
		StateMachineTracer::StateMachineTracer()
			: mMachine(nullptr)
			, mVerbosity(TracerVerbosity::kTransitionsOnly)
			, mEnabled(true)
			, mMaxEntriesPerSecond(100)
			, mEntriesThisSecond(0)
			, mLastSecondTimestamp(0.0f)
		{}

		void StateMachineTracer::Attach(IStateMachineInspectable& machine)
		{
			mMachine = &machine;
			machine.SetTransitionListener(this);
		}

		void StateMachineTracer::Detach()
		{
			if (mMachine)
			{
				mMachine->SetTransitionListener(nullptr);
				mMachine = nullptr;
			}
		}

		void StateMachineTracer::SetVerbosity(TracerVerbosity verbosity)
		{
			mVerbosity = verbosity;
		}

		TracerVerbosity StateMachineTracer::GetVerbosity() const
		{
			return mVerbosity;
		}

		void StateMachineTracer::SetMaxEntriesPerSecond(int maxEntries)
		{
			mMaxEntriesPerSecond = maxEntries;
		}

		void StateMachineTracer::Enable()
		{
			mEnabled = true;
		}

		void StateMachineTracer::Disable()
		{
			mEnabled = false;
		}

		bool StateMachineTracer::IsEnabled() const
		{
			return mEnabled;
		}

		void StateMachineTracer::OnTransition(const TransitionEvent& event)
		{
			if (!mEnabled || !CanLog(event.timestamp))
			{
				return;
			}

			DIA_LOG_INFO("StateMachine",
				"{\"event\":\"transition\",\"source\":\"%s\",\"target\":\"%s\",\"trigger\":\"%s\",\"timestamp\":%.4f}",
				event.sourceStateId.AsChar(),
				event.targetStateId.AsChar(),
				event.triggerId.AsChar(),
				event.timestamp);

			if (mVerbosity >= TracerVerbosity::kTransitionsAndGuards && event.guardCount > 0)
			{
				for (int i = 0; i < event.guardCount; ++i)
				{
					DIA_LOG_INFO("StateMachine",
						"{\"event\":\"guard\",\"name\":\"%s\",\"passed\":%s}",
						event.guardResults[i].guardName.AsChar(),
						event.guardResults[i].passed ? "true" : "false");
				}
			}

			if (mVerbosity >= TracerVerbosity::kFull)
			{
				DIA_LOG_INFO("StateMachine",
					"{\"event\":\"timing\",\"residenceTime\":%.4f}",
					event.stateResidenceTime);
			}
		}

		void StateMachineTracer::OnTransitionFailed(Dia::Core::StringCRC machineId,
			Dia::Core::StringCRC currentStateId,
			Dia::Core::StringCRC triggerId)
		{
			if (!mEnabled || !CanLog(0.0f))
			{
				return;
			}

			DIA_LOG_INFO("StateMachine",
				"{\"event\":\"transition_failed\",\"machine\":\"%s\",\"state\":\"%s\",\"trigger\":\"%s\"}",
				machineId.AsChar(),
				currentStateId.AsChar(),
				triggerId.AsChar());
		}

		bool StateMachineTracer::CanLog(float timestamp)
		{
			if (timestamp - mLastSecondTimestamp >= 1.0f)
			{
				mEntriesThisSecond = 0;
				mLastSecondTimestamp = timestamp;
			}

			++mEntriesThisSecond;
			return mEntriesThisSecond <= mMaxEntriesPerSecond;
		}
	}
}
