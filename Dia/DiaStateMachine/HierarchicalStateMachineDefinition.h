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

		struct HierarchicalStateDef
		{
			Dia::Core::StringCRC id;
			Dia::Core::StringCRC parentId;
			Dia::Core::StringCRC initialChildId;
			bool hasHistory = false;
			void(*onEnter)(void*) = nullptr;
			Dia::Core::StringCRC onEnterName;
			void(*onExit)(void*) = nullptr;
			Dia::Core::StringCRC onExitName;
			void(*onUpdate)(void*, float) = nullptr;
			Dia::Core::StringCRC onUpdateName;
		};

		struct HierarchicalTransitionDef
		{
			Dia::Core::StringCRC sourceStateId;
			Dia::Core::StringCRC targetStateId;
			Dia::Core::StringCRC triggerId;
			bool(*guard)(const void*) = nullptr;
			Dia::Core::StringCRC guardName;
		};

		class HierarchicalStateMachineBuilder;

		class HierarchicalStateMachineDefinition
		{
		public:
			static const unsigned int kMaxStates = 64;
			static const unsigned int kMaxTransitions = 128;

			HierarchicalStateMachineDefinition();
			HierarchicalStateMachineDefinition(HierarchicalStateMachineDefinition&& other);
			HierarchicalStateMachineDefinition& operator=(HierarchicalStateMachineDefinition&& other);

			HierarchicalStateMachineDefinition(const HierarchicalStateMachineDefinition&) = delete;
			HierarchicalStateMachineDefinition& operator=(const HierarchicalStateMachineDefinition&) = delete;

			HierarchicalStateMachineDefinition Clone() const;

			Dia::Core::Containers::DynamicArrayC<HierarchicalStateDef, kMaxStates>& GetStates();
			const Dia::Core::Containers::DynamicArrayC<HierarchicalStateDef, kMaxStates>& GetStates() const;
			Dia::Core::Containers::DynamicArrayC<HierarchicalTransitionDef, kMaxTransitions>& GetTransitions();
			const Dia::Core::Containers::DynamicArrayC<HierarchicalTransitionDef, kMaxTransitions>& GetTransitions() const;
			Dia::Core::StringCRC GetInitialStateId() const;
			void SetInitialStateId(Dia::Core::StringCRC id);

			bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 16>& outErrors) const;
			bool IsValid() const;

			MetadataArray& GetMetadata();
			const MetadataArray& GetMetadata() const;

			MetadataArray& GetStateMetadata(unsigned int stateIndex);
			const MetadataArray& GetStateMetadata(unsigned int stateIndex) const;

		private:
			friend class HierarchicalStateMachineBuilder;
			friend class JsonStateMachineSerializer;

			void MarkValid();

			Dia::Core::Containers::DynamicArrayC<HierarchicalStateDef, kMaxStates> mStates;
			Dia::Core::Containers::DynamicArrayC<HierarchicalTransitionDef, kMaxTransitions> mTransitions;
			Dia::Core::StringCRC mInitialStateId;
			MetadataArray mMetadata;
			Dia::Core::Containers::DynamicArrayC<MetadataArray, kMaxStates> mStateMetadata;
			bool mIsValid = false;
		};
	}
}
