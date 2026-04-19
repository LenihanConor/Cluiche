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
