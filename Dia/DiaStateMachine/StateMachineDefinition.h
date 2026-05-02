#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace StateMachine
	{
		struct StateDef
		{
			Dia::Core::StringCRC id;
			void(*onEnter)(void*) = nullptr;
			void(*onExit)(void*) = nullptr;
			void(*onUpdate)(void*, float) = nullptr;
		};

		struct TransitionDef
		{
			Dia::Core::StringCRC sourceStateId;
			Dia::Core::StringCRC targetStateId;
			Dia::Core::StringCRC triggerId;
			bool(*guard)(const void*) = nullptr;
			Dia::Core::StringCRC guardName;
		};

		extern const Dia::Core::StringCRC kAnyState;

		class StateMachineDefinition
		{
		public:
			static const unsigned int kMaxStates = 64;
			static const unsigned int kMaxTransitions = 128;

			StateMachineDefinition();
			StateMachineDefinition(StateMachineDefinition&& other);
			StateMachineDefinition& operator=(StateMachineDefinition&& other);

			StateMachineDefinition(const StateMachineDefinition&) = delete;
			StateMachineDefinition& operator=(const StateMachineDefinition&) = delete;

			StateMachineDefinition Clone() const;

			const Dia::Core::Containers::DynamicArrayC<StateDef, kMaxStates>& GetStates() const;
			const Dia::Core::Containers::DynamicArrayC<TransitionDef, kMaxTransitions>& GetTransitions() const;
			Dia::Core::StringCRC GetInitialStateId() const;

			bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 16>& outErrors) const;

			bool IsValid() const;

		private:
			friend class StateMachineBuilder;

			Dia::Core::Containers::DynamicArrayC<StateDef, kMaxStates> mStates;
			Dia::Core::Containers::DynamicArrayC<TransitionDef, kMaxTransitions> mTransitions;
			Dia::Core::StringCRC mInitialStateId;
			bool mIsValid = false;
		};
	}
}
