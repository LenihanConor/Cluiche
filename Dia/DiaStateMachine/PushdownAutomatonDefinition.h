#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace StateMachine
	{
		struct PushdownStateDef
		{
			Dia::Core::StringCRC id;
			void(*onEnter)(void*) = nullptr;
			void(*onExit)(void*) = nullptr;
			void(*onUpdate)(void*, float) = nullptr;
			void(*onPause)(void*) = nullptr;
			void(*onResume)(void*) = nullptr;
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

			const Dia::Core::Containers::DynamicArrayC<PushdownStateDef, kMaxStates>& GetStates() const;
			Dia::Core::StringCRC GetInitialStateId() const;

			bool Validate(Dia::Core::Containers::DynamicArrayC<const char*, 16>& outErrors) const;
			bool IsValid() const;

		private:
			friend class PushdownAutomatonBuilder;

			Dia::Core::Containers::DynamicArrayC<PushdownStateDef, kMaxStates> mStates;
			Dia::Core::StringCRC mInitialStateId;
			bool mIsValid = false;
		};
	}
}
