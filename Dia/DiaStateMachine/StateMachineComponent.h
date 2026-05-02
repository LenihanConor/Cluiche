#pragma once

#include "DiaStateMachine/IStateMachineInspectable.h"
#include "DiaCore/Architecture/Components/Interface/IComponent.h"

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

		class StateMachineComponent : public Dia::Core::IComponent
		{
		public:
			COMPONENT_DECLARATION(0x534D0001)

			StateMachineComponent();
			virtual ~StateMachineComponent();

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
