#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/HashTables/HashTableC.h"
#include "DiaCore/CRC/CRCHashFunctor.h"

namespace Dia
{
	namespace StateMachine
	{
		class CallbackRegistry
		{
		public:
			using ActionFn = void(*)(void*);
			using GuardFn = bool(*)(const void*);
			using UpdateFn = void(*)(void*, float);
			using PauseFn = void(*)(void*);

			static const unsigned int kMaxEntries = 128;

			void RegisterAction(Dia::Core::StringCRC name, ActionFn fn);
			void RegisterGuard(Dia::Core::StringCRC name, GuardFn fn);
			void RegisterUpdate(Dia::Core::StringCRC name, UpdateFn fn);

			void Finalize();
			bool IsFinalized() const;

			ActionFn FindAction(Dia::Core::StringCRC name) const;
			GuardFn FindGuard(Dia::Core::StringCRC name) const;
			UpdateFn FindUpdate(Dia::Core::StringCRC name) const;

			bool HasAction(Dia::Core::StringCRC name) const;
			bool HasGuard(Dia::Core::StringCRC name) const;
			bool HasUpdate(Dia::Core::StringCRC name) const;

		private:
			using ActionTable = Dia::Core::Containers::HashTableC<
				Dia::Core::StringCRC, ActionFn, Dia::Core::StringCRCHashFunctor,
				kMaxEntries, kMaxEntries>;
			using GuardTable = Dia::Core::Containers::HashTableC<
				Dia::Core::StringCRC, GuardFn, Dia::Core::StringCRCHashFunctor,
				kMaxEntries, kMaxEntries>;
			using UpdateTable = Dia::Core::Containers::HashTableC<
				Dia::Core::StringCRC, UpdateFn, Dia::Core::StringCRCHashFunctor,
				kMaxEntries, kMaxEntries>;

			ActionTable mActions;
			GuardTable mGuards;
			UpdateTable mUpdates;
			bool mIsFinalized = false;
		};
	}
}
