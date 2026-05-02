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

		struct PushdownStateDef
		{
			Dia::Core::StringCRC id;
			void(*onEnter)(void*) = nullptr;
			Dia::Core::StringCRC onEnterName;
			void(*onExit)(void*) = nullptr;
			Dia::Core::StringCRC onExitName;
			void(*onUpdate)(void*, float) = nullptr;
			Dia::Core::StringCRC onUpdateName;
			void(*onPause)(void*) = nullptr;
			Dia::Core::StringCRC onPauseName;
			void(*onResume)(void*) = nullptr;
			Dia::Core::StringCRC onResumeName;

			MetadataArray metadata;
		};

		class PushdownAutomatonBuilder;

		class PushdownAutomatonDefinition
		{
		public:
			static const unsigned int kMaxStates = 64;

			PushdownAutomatonDefinition();
			PushdownAutomatonDefinition(PushdownAutomatonDefinition&& other);
			PushdownAutomatonDefinition& operator=(PushdownAutomatonDefinition&& other);

			PushdownAutomatonDefinition(const PushdownAutomatonDefinition&) = delete;
			PushdownAutomatonDefinition& operator=(const PushdownAutomatonDefinition&) = delete;

			PushdownAutomatonDefinition Clone() const;

			Dia::Core::Containers::DynamicArrayC<PushdownStateDef, kMaxStates>& GetStates();
			const Dia::Core::Containers::DynamicArrayC<PushdownStateDef, kMaxStates>& GetStates() const;
			Dia::Core::StringCRC GetInitialStateId() const;
			void SetInitialStateId(Dia::Core::StringCRC id);

			bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 16>& outErrors) const;
			bool IsValid() const;

			MetadataArray& GetMetadata();
			const MetadataArray& GetMetadata() const;

		private:
			friend class PushdownAutomatonBuilder;
			friend class JsonStateMachineSerializer;

			void MarkValid();

			Dia::Core::Containers::DynamicArrayC<PushdownStateDef, kMaxStates> mStates;
			Dia::Core::StringCRC mInitialStateId;
			MetadataArray mMetadata;
			bool mIsValid = false;
		};
	}
}
