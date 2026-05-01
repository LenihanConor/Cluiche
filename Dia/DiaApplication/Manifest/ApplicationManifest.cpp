#include "ApplicationManifest.h"

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Application
	{
		// ModuleEntry
		ApplicationManifest::ModuleEntry::ModuleEntry()
			: typeId()
			, instanceId()
			, phaseIds()
			, dependencies()
			, config(nullptr)
		{
		}

		ApplicationManifest::ModuleEntry::ModuleEntry(const ModuleEntry& other)
			: typeId(other.typeId)
			, instanceId(other.instanceId)
			, phaseIds(other.phaseIds)
			, dependencies(other.dependencies)
			, config(other.config ? new Json::Value(*other.config) : nullptr)
		{
		}

		ApplicationManifest::ModuleEntry& ApplicationManifest::ModuleEntry::operator=(const ModuleEntry& other)
		{
			if (this != &other)
			{
				typeId = other.typeId;
				instanceId = other.instanceId;
				phaseIds = other.phaseIds;
				dependencies = other.dependencies;
				delete config;
				config = other.config ? new Json::Value(*other.config) : nullptr;
			}
			return *this;
		}

		ApplicationManifest::ModuleEntry::~ModuleEntry()
		{
			delete config;
		}

		// PhaseEntry
		ApplicationManifest::PhaseEntry::PhaseEntry()
			: typeId()
			, instanceId()
			, config(nullptr)
		{
		}

		ApplicationManifest::PhaseEntry::PhaseEntry(const PhaseEntry& other)
			: typeId(other.typeId)
			, instanceId(other.instanceId)
			, config(other.config ? new Json::Value(*other.config) : nullptr)
		{
		}

		ApplicationManifest::PhaseEntry& ApplicationManifest::PhaseEntry::operator=(const PhaseEntry& other)
		{
			if (this != &other)
			{
				typeId = other.typeId;
				instanceId = other.instanceId;
				delete config;
				config = other.config ? new Json::Value(*other.config) : nullptr;
			}
			return *this;
		}

		ApplicationManifest::PhaseEntry::~PhaseEntry()
		{
			delete config;
		}

		// ProcessingUnitEntry
		ApplicationManifest::ProcessingUnitEntry::ProcessingUnitEntry()
			: typeId()
			, instanceId()
			, frequencyHz(-1.0f)
			, dedicatedThread(false)
			, config(nullptr)
			, phases()
			, transitions()
			, initialPhase()
			, modules()
		{
		}

		ApplicationManifest::ProcessingUnitEntry::ProcessingUnitEntry(const ProcessingUnitEntry& other)
			: typeId(other.typeId)
			, instanceId(other.instanceId)
			, frequencyHz(other.frequencyHz)
			, dedicatedThread(other.dedicatedThread)
			, config(other.config ? new Json::Value(*other.config) : nullptr)
			, phases(other.phases)
			, transitions(other.transitions)
			, initialPhase(other.initialPhase)
			, modules(other.modules)
		{
		}

		ApplicationManifest::ProcessingUnitEntry& ApplicationManifest::ProcessingUnitEntry::operator=(const ProcessingUnitEntry& other)
		{
			if (this != &other)
			{
				typeId = other.typeId;
				instanceId = other.instanceId;
				frequencyHz = other.frequencyHz;
				dedicatedThread = other.dedicatedThread;
				delete config;
				config = other.config ? new Json::Value(*other.config) : nullptr;
				phases = other.phases;
				transitions = other.transitions;
				initialPhase = other.initialPhase;
				modules = other.modules;
			}
			return *this;
		}

		ApplicationManifest::ProcessingUnitEntry::~ProcessingUnitEntry()
		{
			delete config;
		}

		// ApplicationManifest
		ApplicationManifest::ApplicationManifest()
			: version(1)
			, processingUnits()
			, imports()
			, metadata(nullptr)
		{
		}

		ApplicationManifest::~ApplicationManifest()
		{
			// Clean up imports (string literals owned elsewhere)
			imports.RemoveAll();

			delete metadata;
		}
	}
}
