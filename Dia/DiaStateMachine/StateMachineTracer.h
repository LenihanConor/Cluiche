#pragma once

#include "DiaStateMachine/IStateMachineInspectable.h"

namespace Dia
{
	namespace StateMachine
	{
		enum class TracerVerbosity
		{
			kTransitionsOnly,
			kTransitionsAndGuards,
			kFull
		};

		class StateMachineTracer : public ITransitionListener
		{
		public:
			StateMachineTracer();

			void Attach(IStateMachineInspectable& machine);
			void Detach();

			void SetVerbosity(TracerVerbosity verbosity);
			TracerVerbosity GetVerbosity() const;

			void SetMaxEntriesPerSecond(int maxEntries);

			void Enable();
			void Disable();
			bool IsEnabled() const;

			void OnTransition(const TransitionEvent& event) override;
			void OnTransitionFailed(Dia::Core::StringCRC machineId,
				Dia::Core::StringCRC currentStateId,
				Dia::Core::StringCRC triggerId) override;

		private:
			bool CanLog(float timestamp);

			IStateMachineInspectable* mMachine = nullptr;
			TracerVerbosity mVerbosity = TracerVerbosity::kTransitionsOnly;
			bool mEnabled = true;
			int mMaxEntriesPerSecond = 100;
			int mEntriesThisSecond = 0;
			float mLastSecondTimestamp = 0.0f;
		};
	}
}
