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
			: mProcessingUnitFactories(32, 64)
			, mPhaseFactories(64, 128)
			, mModuleFactories(128, 256)
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
				Dia::Core::Log::OutputVaradicLine("Warning: ProcessingUnit type '%s' already registered, skipping duplicate", typeId.AsChar());
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
				Dia::Core::Log::OutputVaradicLine("Warning: Phase type '%s' already registered, skipping duplicate", typeId.AsChar());
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
				Dia::Core::Log::OutputVaradicLine("Warning: Module type '%s' already registered, skipping duplicate", typeId.AsChar());
				return;
			}

			mModuleFactories.Add(typeId, factory);
			mTypeListsDirty = true;
		}

		ProcessingUnit* ApplicationTypeRegistry::CreateProcessingUnit(const Dia::Core::StringCRC& typeId,
			const Dia::Core::StringCRC& instanceId,
			const Json::Value& config)
		{
			ITypeFactory<ProcessingUnit>** ppFactory = mProcessingUnitFactories.TryGetItem(typeId);
			if (ppFactory == nullptr || *ppFactory == nullptr)
			{
				Dia::Core::Log::OutputVaradicLine("Error: ProcessingUnit type '%s' not registered", typeId.AsChar());
				return nullptr;
			}

			return (*ppFactory)->Create(instanceId, config);
		}

		Phase* ApplicationTypeRegistry::CreatePhase(const Dia::Core::StringCRC& typeId,
			ProcessingUnit* pu,
			const Dia::Core::StringCRC& instanceId,
			const Json::Value& config)
		{
			DIA_ASSERT(pu != nullptr, "ProcessingUnit cannot be null");

			ITypeFactory<Phase>** ppFactory = mPhaseFactories.TryGetItem(typeId);
			if (ppFactory == nullptr || *ppFactory == nullptr)
			{
				Dia::Core::Log::OutputVaradicLine("Error: Phase type '%s' not registered", typeId.AsChar());
				return nullptr;
			}

			return (*ppFactory)->Create(pu, instanceId, config);
		}

		Module* ApplicationTypeRegistry::CreateModule(const Dia::Core::StringCRC& typeId,
			ProcessingUnit* pu,
			const Dia::Core::StringCRC& instanceId,
			const Json::Value& config)
		{
			DIA_ASSERT(pu != nullptr, "ProcessingUnit cannot be null");

			ITypeFactory<Module>** ppFactory = mModuleFactories.TryGetItem(typeId);
			if (ppFactory == nullptr || *ppFactory == nullptr)
			{
				Dia::Core::Log::OutputVaradicLine("Error: Module type '%s' not registered", typeId.AsChar());
				return nullptr;
			}

			return (*ppFactory)->Create(pu, instanceId, config);
		}

		const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& ApplicationTypeRegistry::GetRegisteredProcessingUnitTypes() const
		{
			if (mTypeListsDirty)
			{
				RebuildTypeLists();
			}
			return mProcessingUnitTypes;
		}

		const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64>& ApplicationTypeRegistry::GetRegisteredPhaseTypes() const
		{
			if (mTypeListsDirty)
			{
				RebuildTypeLists();
			}
			return mPhaseTypes;
		}

		const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& ApplicationTypeRegistry::GetRegisteredModuleTypes() const
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
			mProcessingUnitTypes.RemoveAll();
			mPhaseTypes.RemoveAll();
			mModuleTypes.RemoveAll();

			for (auto it = mProcessingUnitFactories.Begin(); it != mProcessingUnitFactories.End(); ++it)
			{
				mProcessingUnitTypes.Add(it.Key());
			}

			for (auto it = mPhaseFactories.Begin(); it != mPhaseFactories.End(); ++it)
			{
				mPhaseTypes.Add(it.Key());
			}

			for (auto it = mModuleFactories.Begin(); it != mModuleFactories.End(); ++it)
			{
				mModuleTypes.Add(it.Key());
			}

			mTypeListsDirty = false;
		}
	}
}
