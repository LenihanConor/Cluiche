#include "DiaApplicationEditor/ManifestSerializer.h"
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Application::Editor;
using namespace Dia::Application;

bool ManifestSerializer::Serialize(const ApplicationManifest& manifest, Json::Value& outJson)
{
	outJson = Json::Value(Json::objectValue);
	outJson["version"] = manifest.version;

	Json::Value& pusJson = outJson["processing_units"] = Json::Value(Json::arrayValue);
	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		Json::Value puJson;
		SerializeProcessingUnit(manifest.processingUnits[i], puJson);
		pusJson.append(puJson);
	}

	if (manifest.imports.Size() > 0)
	{
		Json::Value& importsJson = outJson["imports"] = Json::Value(Json::arrayValue);
		for (unsigned int i = 0; i < manifest.imports.Size(); ++i)
		{
			if (manifest.imports[i] != nullptr)
				importsJson.append(manifest.imports[i]);
		}
	}

	return true;
}

void ManifestSerializer::SerializeProcessingUnit(const ApplicationManifest::ProcessingUnitEntry& pu, Json::Value& outJson)
{
	outJson["type"] = pu.typeId.AsChar();
	outJson["instance_id"] = pu.instanceId.AsChar();
	outJson["frequency_hz"] = pu.frequencyHz;
	outJson["dedicated_thread"] = pu.dedicatedThread;

	if (pu.initialPhase != Dia::Core::StringCRC::kZero)
		outJson["initial_phase"] = pu.initialPhase.AsChar();

	if (pu.config != nullptr)
		outJson["config"] = *pu.config;

	Json::Value& phasesJson = outJson["phases"] = Json::Value(Json::arrayValue);
	for (unsigned int i = 0; i < pu.phases.Size(); ++i)
	{
		Json::Value phaseJson;
		SerializePhase(pu.phases[i], phaseJson);
		phasesJson.append(phaseJson);
	}

	Json::Value& transitionsJson = outJson["transitions"] = Json::Value(Json::arrayValue);
	for (unsigned int i = 0; i < pu.transitions.Size(); ++i)
	{
		Json::Value tJson;
		tJson["from"] = pu.transitions[i].fromPhase.AsChar();
		tJson["to"] = pu.transitions[i].toPhase.AsChar();
		transitionsJson.append(tJson);
	}

	Json::Value& modulesJson = outJson["modules"] = Json::Value(Json::arrayValue);
	for (unsigned int i = 0; i < pu.modules.Size(); ++i)
	{
		Json::Value moduleJson;
		SerializeModule(pu.modules[i], moduleJson);
		modulesJson.append(moduleJson);
	}
}

void ManifestSerializer::SerializePhase(const ApplicationManifest::PhaseEntry& phase, Json::Value& outJson)
{
	outJson["type"] = phase.typeId.AsChar();
	outJson["instance_id"] = phase.instanceId.AsChar();

	if (phase.config != nullptr)
		outJson["config"] = *phase.config;
}

void ManifestSerializer::SerializeModule(const ApplicationManifest::ModuleEntry& module, Json::Value& outJson)
{
	outJson["type"] = module.typeId.AsChar();
	outJson["instance_id"] = module.instanceId.AsChar();

	if (module.phaseIds.Size() > 0)
	{
		Json::Value& phaseIdsJson = outJson["phases"] = Json::Value(Json::arrayValue);
		for (unsigned int i = 0; i < module.phaseIds.Size(); ++i)
			phaseIdsJson.append(module.phaseIds[i].AsChar());
	}

	if (module.dependencies.Size() > 0)
	{
		Json::Value& depsJson = outJson["dependencies"] = Json::Value(Json::arrayValue);
		for (unsigned int i = 0; i < module.dependencies.Size(); ++i)
			depsJson.append(module.dependencies[i].AsChar());
	}

	if (module.config != nullptr)
		outJson["config"] = *module.config;
}
