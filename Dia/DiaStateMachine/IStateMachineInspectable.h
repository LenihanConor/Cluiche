#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace StateMachine
	{
		struct StateInfo
		{
			Dia::Core::StringCRC id;
			bool isActive = false;
			bool isLeaf = true;
			Dia::Core::StringCRC parentId;
		};

		struct TransitionInfo
		{
			Dia::Core::StringCRC sourceStateId;
			Dia::Core::StringCRC targetStateId;
			Dia::Core::StringCRC triggerId;
			bool hasGuard = false;
		};

		struct TransitionRecord
		{
			Dia::Core::StringCRC sourceStateId;
			Dia::Core::StringCRC targetStateId;
			Dia::Core::StringCRC triggerId;
			float timestamp = 0.0f;
		};

		struct GuardEvaluation
		{
			Dia::Core::StringCRC guardName;
			bool passed = false;
		};

		struct TransitionEvent
		{
			Dia::Core::StringCRC sourceStateId;
			Dia::Core::StringCRC targetStateId;
			Dia::Core::StringCRC triggerId;
			float timestamp = 0.0f;
			float stateResidenceTime = 0.0f;
			const GuardEvaluation* guardResults = nullptr;
			int guardCount = 0;
		};

		class ITransitionListener
		{
		public:
			virtual ~ITransitionListener() = default;
			virtual void OnTransition(const TransitionEvent& event) = 0;
			virtual void OnTransitionFailed(Dia::Core::StringCRC machineId,
				Dia::Core::StringCRC currentStateId,
				Dia::Core::StringCRC triggerId) = 0;
		};

		class IStateMachineInspectable
		{
		public:
			virtual ~IStateMachineInspectable() = default;

			virtual Dia::Core::StringCRC GetMachineId() const = 0;
			virtual Dia::Core::StringCRC GetCurrentStateId() const = 0;

			virtual void GetAllStates(
				Dia::Core::Containers::DynamicArrayC<StateInfo, 64>& outStates) const = 0;
			virtual void GetAllTransitions(
				Dia::Core::Containers::DynamicArrayC<TransitionInfo, 64>& outTransitions) const = 0;
			virtual void GetTransitionHistory(
				Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32>& outHistory) const = 0;

			virtual void SetTransitionListener(ITransitionListener* listener) = 0;
		};
	}
}
