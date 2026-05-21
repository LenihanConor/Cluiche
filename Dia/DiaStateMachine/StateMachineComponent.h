#pragma once

#include "DiaStateMachine/IStateMachineInspectable.h"

namespace Dia
{
	namespace StateMachine
	{
		enum class MachineType
		{
			kNone,
			kFlat,
			kHierarchical,
			kPushdown
		};

		class StateMachineComponent
		{
		public:
			StateMachineComponent();
			~StateMachineComponent();

			void AttachMachine(IStateMachineInspectable* machine, MachineType type);

			IStateMachineInspectable* GetInspectable() const;
			MachineType GetMachineType() const;

			template<typename T>
			T* GetMachine() const;

		private:
			IStateMachineInspectable* mMachine = nullptr;
			MachineType mMachineType = MachineType::kNone;
		};

		template<typename T>
		T* StateMachineComponent::GetMachine() const
		{
			return static_cast<T*>(mMachine);
		}
	}
}
