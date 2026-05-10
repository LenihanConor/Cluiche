#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
		class Phase;
		class Module;

		// Factory interface for creating registered types
		template <typename T>
		class ITypeFactory
		{
		public:
			virtual ~ITypeFactory() = default;
			virtual T* Create(const Dia::Core::StringCRC& instanceId, const Json::Value& config) = 0;
		};

		// Specialized factory interface for Phase (needs ProcessingUnit)
		template <>
		class ITypeFactory<Phase>
		{
		public:
			virtual ~ITypeFactory() = default;
			virtual Phase* Create(ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) = 0;
		};

		// Specialized factory interface for Module (needs ProcessingUnit)
		template <>
		class ITypeFactory<Module>
		{
		public:
			virtual ~ITypeFactory() = default;
			virtual Module* Create(ProcessingUnit* pu, const Dia::Core::StringCRC& instanceId, const Json::Value& config) = 0;
		};

		// ---------------------------------------------------------------------------
		// Pending registration queue
		//
		// DIA_REGISTER_* macros run at static-init time, before any ApplicationTypeRegistry
		// instance exists. They enqueue here instead of writing to a singleton.
		// Call ApplicationTypeRegistry::DrainPendingRegistrations() on a live instance
		// at application startup to transfer all queued entries.
		// ---------------------------------------------------------------------------
		enum class PendingRegistrationKind { ProcessingUnit, Phase, Module };

		struct PendingRegistration
		{
			PendingRegistrationKind         kind;
			Dia::Core::StringCRC            typeId;
			void*                           factory; // ITypeFactory<ProcessingUnit/Phase/Module>*
		};

		static const unsigned int kMaxPendingRegistrations = 256;

		// Plain POD struct — no constructor, safe to use at static-init time
		struct PendingRegistrationQueue
		{
			PendingRegistration entries[kMaxPendingRegistrations];
			unsigned int        count;
		};

		PendingRegistrationQueue& GetPendingRegistrationQueue();

		// Central registry for all ProcessingUnit/Phase/Module types
		class ApplicationTypeRegistry
		{
		public:
			ApplicationTypeRegistry();
			~ApplicationTypeRegistry();

			// Non-copyable
			ApplicationTypeRegistry(const ApplicationTypeRegistry&) = delete;
			ApplicationTypeRegistry& operator=(const ApplicationTypeRegistry&) = delete;

			// Drain all pending static-init registrations into this instance.
			// Call once at application startup after constructing the registry.
			void DrainPendingRegistrations();

			// Registration (called directly or via DrainPendingRegistrations)
			void RegisterProcessingUnitType(const Dia::Core::StringCRC& typeId, ITypeFactory<ProcessingUnit>* factory);
			void RegisterPhaseType(const Dia::Core::StringCRC& typeId, ITypeFactory<Phase>* factory);
			void RegisterModuleType(const Dia::Core::StringCRC& typeId, ITypeFactory<Module>* factory);

			// Editor-only stub registration — marks a type as known without providing a factory.
			// IsXxxTypeRegistered() returns true; Create() will return nullptr for stub types.
			void RegisterKnownProcessingUnitType(const Dia::Core::StringCRC& typeId);
			void RegisterKnownPhaseType(const Dia::Core::StringCRC& typeId);
			void RegisterKnownModuleType(const Dia::Core::StringCRC& typeId);

			// Instantiation
			ProcessingUnit* CreateProcessingUnit(const Dia::Core::StringCRC& typeId,
				const Dia::Core::StringCRC& instanceId,
				const Json::Value& config);

			Phase* CreatePhase(const Dia::Core::StringCRC& typeId,
				ProcessingUnit* pu,
				const Dia::Core::StringCRC& instanceId,
				const Json::Value& config);

			Module* CreateModule(const Dia::Core::StringCRC& typeId,
				ProcessingUnit* pu,
				const Dia::Core::StringCRC& instanceId,
				const Json::Value& config);

			// Introspection (for editor)
			const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& GetRegisteredProcessingUnitTypes() const;
			const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64>& GetRegisteredPhaseTypes() const;
			const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& GetRegisteredModuleTypes() const;

			bool IsProcessingUnitTypeRegistered(const Dia::Core::StringCRC& typeId) const;
			bool IsPhaseTypeRegistered(const Dia::Core::StringCRC& typeId) const;
			bool IsModuleTypeRegistered(const Dia::Core::StringCRC& typeId) const;

		private:
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, ITypeFactory<ProcessingUnit>*> mProcessingUnitFactories;
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, ITypeFactory<Phase>*> mPhaseFactories;
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, ITypeFactory<Module>*> mModuleFactories;

			// Cached lists for introspection
			mutable Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> mProcessingUnitTypes;
			mutable Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> mPhaseTypes;
			mutable Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128> mModuleTypes;
			mutable bool mTypeListsDirty;

			void RebuildTypeLists() const;
		};
	}
}
