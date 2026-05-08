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
			, sourceManifestPath()
		{
		}

		ApplicationManifest::ModuleEntry::ModuleEntry(const ModuleEntry& other)
			: typeId(other.typeId)
			, instanceId(other.instanceId)
			, phaseIds(other.phaseIds)
			, dependencies(other.dependencies)
			, config(other.config ? new Json::Value(*other.config) : nullptr)
			, sourceManifestPath(other.sourceManifestPath)
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
				sourceManifestPath = other.sourceManifestPath;
			}
			return *this;
		}

		ApplicationManifest::ModuleEntry::~ModuleEntry()
		{
			delete config;
			config = nullptr;
		}

		// PhaseEntry
		ApplicationManifest::PhaseEntry::PhaseEntry()
			: typeId()
			, instanceId()
			, config(nullptr)
			, sourceManifestPath()
		{
		}

		ApplicationManifest::PhaseEntry::PhaseEntry(const PhaseEntry& other)
			: typeId(other.typeId)
			, instanceId(other.instanceId)
			, config(other.config ? new Json::Value(*other.config) : nullptr)
			, sourceManifestPath(other.sourceManifestPath)
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
				sourceManifestPath = other.sourceManifestPath;
			}
			return *this;
		}

		ApplicationManifest::PhaseEntry::~PhaseEntry()
		{
			delete config;
			config = nullptr;
		}

		// ProcessingUnitEntry
		ApplicationManifest::ProcessingUnitEntry::ProcessingUnitEntry()
			: typeId()
			, instanceId()
			, frequencyHz(-1.0f)
			, dedicatedThread(false)
			, root(false)
			, config(nullptr)
			, sourceManifestPath()
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
			, root(other.root)
			, config(other.config ? new Json::Value(*other.config) : nullptr)
			, sourceManifestPath(other.sourceManifestPath)
			, phases()
			, transitions(other.transitions)
			, initialPhase(other.initialPhase)
			, modules()
		{
			for (unsigned int i = 0; i < other.phases.Size(); ++i)
				phases.Add(other.phases[i]);
			for (unsigned int i = 0; i < other.modules.Size(); ++i)
				modules.Add(other.modules[i]);
		}

		ApplicationManifest::ProcessingUnitEntry& ApplicationManifest::ProcessingUnitEntry::operator=(const ProcessingUnitEntry& other)
		{
			if (this != &other)
			{
				typeId = other.typeId;
				instanceId = other.instanceId;
				frequencyHz = other.frequencyHz;
				dedicatedThread = other.dedicatedThread;
				root = other.root;
				delete config;
				config = other.config ? new Json::Value(*other.config) : nullptr;
				sourceManifestPath = other.sourceManifestPath;
				// RemoveAll calls ~PhaseEntry() which nulls config; Add calls operator= which
				// safely deletes null. Safe because destructors null their pointers.
				phases.RemoveAll();
				for (unsigned int i = 0; i < other.phases.Size(); ++i)
					phases.Add(other.phases[i]);
				transitions = other.transitions;
				initialPhase = other.initialPhase;
				modules.RemoveAll();
				for (unsigned int i = 0; i < other.modules.Size(); ++i)
					modules.Add(other.modules[i]);
			}
			return *this;
		}

		ApplicationManifest::ProcessingUnitEntry::~ProcessingUnitEntry()
		{
			delete config;
			config = nullptr;
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
			delete metadata;
		}
	}
}
