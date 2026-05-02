#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include <DiaSerializer/MetadataValue.h>

namespace Dia
{
	namespace StateMachine
	{
		using MetadataArray = Dia::Serializer::MetadataArray;
		using MetadataValue = Dia::Serializer::MetadataValue;
		using MetadataEntry = Dia::Serializer::MetadataEntry;
		using Dia::Serializer::SetMetadata;
		using Dia::Serializer::FindMetadata;

		struct StateDef
		{
			Dia::Core::StringCRC id;
			void(*onEnter)(void*) = nullptr;
			Dia::Core::StringCRC onEnterName;
			void(*onExit)(void*) = nullptr;
			Dia::Core::StringCRC onExitName;
			void(*onUpdate)(void*, float) = nullptr;
			Dia::Core::StringCRC onUpdateName;

			MetadataArray metadata;
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

			Dia::Core::Containers::DynamicArrayC<StateDef, kMaxStates>& GetStates();
			const Dia::Core::Containers::DynamicArrayC<StateDef, kMaxStates>& GetStates() const;
			Dia::Core::Containers::DynamicArrayC<TransitionDef, kMaxTransitions>& GetTransitions();
			const Dia::Core::Containers::DynamicArrayC<TransitionDef, kMaxTransitions>& GetTransitions() const;
			Dia::Core::StringCRC GetInitialStateId() const;
			void SetInitialStateId(Dia::Core::StringCRC id);

			bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 16>& outErrors) const;

			bool IsValid() const;

			MetadataArray& GetMetadata();
			const MetadataArray& GetMetadata() const;

		private:
			friend class StateMachineBuilder;
			friend class JsonStateMachineSerializer;

			void MarkValid();

			Dia::Core::Containers::DynamicArrayC<StateDef, kMaxStates> mStates;
			Dia::Core::Containers::DynamicArrayC<TransitionDef, kMaxTransitions> mTransitions;
			Dia::Core::StringCRC mInitialStateId;
			MetadataArray mMetadata;
			bool mIsValid = false;
		};
	}
}
