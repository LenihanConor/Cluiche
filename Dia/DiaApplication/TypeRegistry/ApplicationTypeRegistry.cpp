#include "ApplicationTypeRegistry.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace Application
	{
		ApplicationTypeRegistry& ApplicationTypeRegistry::Instance()
		{
			static ApplicationTypeRegistry instance;
			return instance;
		}

		ApplicationTypeRegistry::ApplicationTypeRegistry()
			: mProcessingUnitFactories(32)
			, mPhaseFactories(64)
			, mModuleFactories(128)
			, mProcessingUnitTypes()
			, mPhaseTypes()
			, mModuleTypes()
			, mTypeListsDirty(true)
		{
		}

		ApplicationTypeRegistry::~ApplicationTypeRegistry()
		{
			// Factories are owned by static registration objects, not deleted here
		}

		void ApplicationTypeRegistry::RegisterProcessingUnitType(const Dia::Core::StringCRC& typeId, ITypeFactory<ProcessingUnit>* factory)
		{
			DIA_ASSERT(factory != nullptr, "Factory cannot be null");

			if (mProcessingUnitFactories.ContainsKey(typeId))
			{
				DIA_LOG("Warning: ProcessingUnit type '%s' already registered, skipping duplicate", typeId.AsChar());
				return;
			}

			mProcessingUnitFactories.Add(typeId, factory);
			mTypeListsDirty = true;
		}

		void ApplicationTypeRegistry::RegisterPhaseType(const Dia::Core::StringCRC& typeId, ITypeFactory<Phase>* factory)
		{
			DIA_ASSERT(factory != nullptr, "Factory cannot be null");

			if (mPhaseFactories.ContainsKey(typeId))
			{
				DIA_LOG("Warning: Phase type '%s' already registered, skipping duplicate", typeId.AsChar());
				return;
			}

			mPhaseFactories.Add(typeId, factory);
			mTypeListsDirty = true;
		}

		void ApplicationTypeRegistry::RegisterModuleType(const Dia::Core::StringCRC& typeId, ITypeFactory<Module>* factory)
		{
			DIA_ASSERT(factory != nullptr, "Factory cannot be null");

			if (mModuleFactories.ContainsKey(typeId))
			{
				DIA_LOG("Warning: Module type '%s' already registered, skipping duplicate", typeId.AsChar());
				return;
			}

			mModuleFactories.Add(typeId, factory);
			mTypeListsDirty = true;
		}

		ProcessingUnit* ApplicationTypeRegistry::CreateProcessingUnit(const Dia::Core::StringCRC& typeId,
			const Dia::Core::StringCRC& instanceId,
			const Json::Value& config)
		{
			ITypeFactory<ProcessingUnit>* factory = mProcessingUnitFactories.At(typeId);
			if (factory == nullptr)
			{
				DIA_LOG("Error: ProcessingUnit type '%s' not registered", typeId.AsChar());
				return nullptr;
			}

			return factory->Create(instanceId, config);
		}

		Phase* ApplicationTypeRegistry::CreatePhase(const Dia::Core::StringCRC& typeId,
			ProcessingUnit* pu,
			const Dia::Core::StringCRC& instanceId,
			const Json::Value& config)
		{
			DIA_ASSERT(pu != nullptr, "ProcessingUnit cannot be null");

			ITypeFactory<Phase>* factory = mPhaseFactories.At(typeId);
			if (factory == nullptr)
			{
				DIA_LOG("Error: Phase type '%s' not registered", typeId.AsChar());
				return nullptr;
			}

			return factory->Create(pu, instanceId, config);
		}

		Module* ApplicationTypeRegistry::CreateModule(const Dia::Core::StringCRC& typeId,
			ProcessingUnit* pu,
			const Dia::Core::StringCRC& instanceId,
			const Json::Value& config)
		{
			DIA_ASSERT(pu != nullptr, "ProcessingUnit cannot be null");

			ITypeFactory<Module>* factory = mModuleFactories.At(typeId);
			if (factory == nullptr)
			{
				DIA_LOG("Error: Module type '%s' not registered", typeId.AsChar());
				return nullptr;
			}

			return factory->Create(pu, instanceId, config);
		}

		const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC>& ApplicationTypeRegistry::GetRegisteredProcessingUnitTypes() const
		{
			if (mTypeListsDirty)
			{
				RebuildTypeLists();
			}
			return mProcessingUnitTypes;
		}

		const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC>& ApplicationTypeRegistry::GetRegisteredPhaseTypes() const
		{
			if (mTypeListsDirty)
			{
				RebuildTypeLists();
			}
			return mPhaseTypes;
		}

		const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC>& ApplicationTypeRegistry::GetRegisteredModuleTypes() const
		{
			if (mTypeListsDirty)
			{
				RebuildTypeLists();
			}
			return mModuleTypes;
		}

		bool ApplicationTypeRegistry::IsProcessingUnitTypeRegistered(const Dia::Core::StringCRC& typeId) const
		{
			return mProcessingUnitFactories.ContainsKey(typeId);
		}

		bool ApplicationTypeRegistry::IsPhaseTypeRegistered(const Dia::Core::StringCRC& typeId) const
		{
			return mPhaseFactories.ContainsKey(typeId);
		}

		bool ApplicationTypeRegistry::IsModuleTypeRegistered(const Dia::Core::StringCRC& typeId) const
		{
			return mModuleFactories.ContainsKey(typeId);
		}

		void ApplicationTypeRegistry::RebuildTypeLists() const
		{
			mProcessingUnitTypes.Clear();
			mPhaseTypes.Clear();
			mModuleTypes.Clear();

			// Build ProcessingUnit type list
			auto puIter = mProcessingUnitFactories.IteratorAt(0);
			while (puIter.IsValid())
			{
				mProcessingUnitTypes.Add(puIter.Current().mKey);
				puIter.Next();
			}

			// Build Phase type list
			auto phaseIter = mPhaseFactories.IteratorAt(0);
			while (phaseIter.IsValid())
			{
				mPhaseTypes.Add(phaseIter.Current().mKey);
				phaseIter.Next();
			}

			// Build Module type list
			auto moduleIter = mModuleFactories.IteratorAt(0);
			while (moduleIter.IsValid())
			{
				mModuleTypes.Add(moduleIter.Current().mKey);
				moduleIter.Next();
			}

			mTypeListsDirty = false;
		}
	}
}
