#include "DiaStateMachine/StateMachineComponent.h"

namespace Dia
{
	namespace StateMachine
	{
		StateMachineComponent::StateMachineComponent()
			: mMachine(nullptr)
			, mMachineType(MachineType::kNone)
		{}

		StateMachineComponent::~StateMachineComponent()
		{}

		void StateMachineComponent::AttachMachine(IStateMachineInspectable* machine, MachineType type)
		{
			mMachine = machine;
			mMachineType = type;
		}

		IStateMachineInspectable* StateMachineComponent::GetInspectable() const
		{
			return mMachine;
		}

		MachineType StateMachineComponent::GetMachineType() const
		{
			return mMachineType;
		}
	}
}
