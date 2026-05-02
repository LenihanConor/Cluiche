#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaStateMachine/StateMachineMetadata.h"

namespace Dia
{
	namespace StateMachine
	{
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

			MetadataArray metadata;
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

		private:
			friend class HierarchicalStateMachineBuilder;

			Dia::Core::Containers::DynamicArrayC<HierarchicalStateDef, kMaxStates> mStates;
			Dia::Core::Containers::DynamicArrayC<HierarchicalTransitionDef, kMaxTransitions> mTransitions;
			Dia::Core::StringCRC mInitialStateId;
			MetadataArray mMetadata;
			bool mIsValid = false;
		};
	}
}
