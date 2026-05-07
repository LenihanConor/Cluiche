#include "DiaApplicationEditor/DiaApplicationEditor.h"
#include "DiaApplicationEditor/ManifestSerializer.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/UI/FileDialogHandler.h>

#include <DiaApplication/Manifest/ApplicationManifestLoader.h>
#include <DiaApplication/Manifest/JsonApplicationManifestSerializer.h>
#include <DiaApplication/Manifest/ManifestComposer.h>
#include <DiaApplication/Manifest/ManifestValidator.h>
#include <DiaApplication/Manifest/DiaGameManifest.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>

#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <cstring>
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace Dia::Application::Editor;
using namespace Dia::Application;
using namespace Dia::Core;

DiaApplicationEditor::DiaApplicationEditor()
	: mData(nullptr)
	, mBridge(nullptr)
	, mIsSaving(false)
{
}

DiaApplicationEditor::~DiaApplicationEditor()
{
	delete mData;
}

void DiaApplicationEditor::OnLoad(const Dia::Editor::EditorPluginContext& context)
{
	mData = new ManifestEditorData();
	mBridge = context.mBridge;

	if (mBridge != nullptr)
	{
		mBridge->RegisterEventHandler(StringCRC("open_manifest"), [this](const Json::Value& data) {
			OpenManifest(data.isMember("path") ? data["path"].asCString() : nullptr);
		});
		mBridge->RegisterEventHandler(StringCRC("save_manifest"), [this](const Json::Value&) {
			SaveManifest();
		});
		mBridge->RegisterEventHandler(StringCRC("save_manifest_as"), [this](const Json::Value& data) {
			SaveManifestAs(data.isMember("path") ? data["path"].asCString() : nullptr);
		});
		mBridge->RegisterEventHandler(StringCRC("close_manifest"), [this](const Json::Value&) {
			CloseManifest();
		});
		mBridge->RegisterEventHandler(StringCRC("validate"), [this](const Json::Value&) {
			ValidateManifest();
		});
		mBridge->RegisterEventHandler(StringCRC("refresh_types"), [this](const Json::Value&) {
			RefreshAvailableTypes();
		});
		mBridge->RegisterEventHandler(StringCRC("add_node"), [this](const Json::Value& data) {
			HandleAddNode(data);
		});
		mBridge->RegisterEventHandler(StringCRC("add_node_confirmed"), [this](const Json::Value& data) {
			HandleAddNodeConfirmed(data);
		});
		mBridge->RegisterEventHandler(StringCRC("remove_node"), [this](const Json::Value& data) {
			HandleRemoveNode(data);
		});
		mBridge->RegisterEventHandler(StringCRC("reorder_node"), [this](const Json::Value& data) {
			HandleReorderNode(data);
		});
		mBridge->RegisterEventHandler(StringCRC("phase_positions_changed"), [this](const Json::Value& data) {
			HandlePhasePositionsChanged(data);
		});
		mBridge->RegisterEventHandler(StringCRC("transition_added"), [this](const Json::Value& data) {
			HandleTransitionAdded(data);
		});
		mBridge->RegisterEventHandler(StringCRC("transition_removed"), [this](const Json::Value& data) {
			HandleTransitionRemoved(data);
		});
		mBridge->RegisterEventHandler(StringCRC("transition_condition_changed"), [this](const Json::Value& data) {
			HandleTransitionConditionChanged(data);
		});
		mBridge->RegisterEventHandler(StringCRC("module_config_changed"), [this](const Json::Value& data) {
			HandleModuleConfigChanged(data);
		});
		mBridge->RegisterEventHandler(StringCRC("resolve_conflict"), [this](const Json::Value& data) {
			HandleResolveConflict(data);
		});
		mBridge->RegisterEventHandler(StringCRC("node_selected"), [this](const Json::Value& data) {
			if (mData != nullptr && data.isMember("node_id"))
				strncpy_s(mData->selectedNodeId, sizeof(mData->selectedNodeId), data["node_id"].asCString(), _TRUNCATE);
		});
		mBridge->RegisterEventHandler(StringCRC("pu_property_changed"), [this](const Json::Value& data) {
			HandlePUPropertyChanged(data);
		});
		mBridge->RegisterEventHandler(StringCRC("phase_property_changed"), [this](const Json::Value& data) {
			HandlePhasePropertyChanged(data);
		});
		mBridge->RegisterEventHandler(StringCRC("initial_phase_changed"), [this](const Json::Value& data) {
			HandleInitialPhaseChanged(data);
		});
		mBridge->RegisterEventHandler(StringCRC("module_phases_changed"), [this](const Json::Value& data) {
			HandleModulePhasesChanged(data);
		});
		mBridge->RegisterEventHandler(StringCRC("new_manifest"), [this](const Json::Value&) {
			HandleNewManifest();
		});
		mBridge->RegisterEventHandler(StringCRC("manifest_restored"), [this](const Json::Value& data) {
			HandleManifestRestored(data);
		});
		mBridge->RegisterEventHandler(StringCRC("add_import"), [this](const Json::Value& data) {
			HandleAddImport(data);
		});
		mBridge->RegisterEventHandler(StringCRC("remove_import"), [this](const Json::Value& data) {
			HandleRemoveImport(data);
		});
	}

	mFileWatcher.Start();
	RefreshAvailableTypes();
}

void DiaApplicationEditor::OnUnload()
{
	mFileWatcher.Stop();

	if (mBridge != nullptr)
	{
		mBridge->UnregisterEventHandler(StringCRC("open_manifest"));
		mBridge->UnregisterEventHandler(StringCRC("save_manifest"));
		mBridge->UnregisterEventHandler(StringCRC("save_manifest_as"));
		mBridge->UnregisterEventHandler(StringCRC("close_manifest"));
		mBridge->UnregisterEventHandler(StringCRC("validate"));
		mBridge->UnregisterEventHandler(StringCRC("refresh_types"));
		mBridge->UnregisterEventHandler(StringCRC("add_node"));
		mBridge->UnregisterEventHandler(StringCRC("add_node_confirmed"));
		mBridge->UnregisterEventHandler(StringCRC("remove_node"));
		mBridge->UnregisterEventHandler(StringCRC("reorder_node"));
		mBridge->UnregisterEventHandler(StringCRC("phase_positions_changed"));
		mBridge->UnregisterEventHandler(StringCRC("transition_added"));
		mBridge->UnregisterEventHandler(StringCRC("transition_removed"));
		mBridge->UnregisterEventHandler(StringCRC("transition_condition_changed"));
		mBridge->UnregisterEventHandler(StringCRC("module_config_changed"));
		mBridge->UnregisterEventHandler(StringCRC("resolve_conflict"));
		mBridge->UnregisterEventHandler(StringCRC("node_selected"));
		mBridge->UnregisterEventHandler(StringCRC("pu_property_changed"));
		mBridge->UnregisterEventHandler(StringCRC("phase_property_changed"));
		mBridge->UnregisterEventHandler(StringCRC("initial_phase_changed"));
		mBridge->UnregisterEventHandler(StringCRC("module_phases_changed"));
		mBridge->UnregisterEventHandler(StringCRC("new_manifest"));
		mBridge->UnregisterEventHandler(StringCRC("manifest_restored"));
		mBridge->UnregisterEventHandler(StringCRC("add_import"));
		mBridge->UnregisterEventHandler(StringCRC("remove_import"));
		mBridge = nullptr;
	}

	delete mData;
	mData = nullptr;
}

void DiaApplicationEditor::OnUpdate(float /*deltaTime*/)
{
	mFileWatcher.Update();
}

void DiaApplicationEditor::OpenManifest(const char* path)
{
	char dialogPath[MAX_PATH] = {};
	if (path == nullptr || path[0] == '\0')
	{
		Json::Value dialogData;
		Json::Value filters(Json::arrayValue);
		Json::Value f1; f1["name"] = "Dia Game Project"; f1["ext"] = "*.diagame"; filters.append(f1);
		Json::Value f2; f2["name"] = "Dia App Manifest"; f2["ext"] = "*.diaapp"; filters.append(f2);
		Json::Value f3; f3["name"] = "All Files"; f3["ext"] = "*.*"; filters.append(f3);
		dialogData["filters"] = filters;
		dialogData["default_ext"] = "diagame";
		dialogData["title"] = "Open Manifest";

		Json::Value result = Dia::Editor::FileDialogHandler::HandleOpenFileDialog(dialogData);
		if (!result.get("success", false).asBool())
			return;
		strncpy_s(dialogPath, MAX_PATH, result["path"].asCString(), _TRUNCATE);
		path = dialogPath;
	}

	// Check for unsaved changes — notify UI to prompt user
	if (IsDirty())
	{
		NotifyUI("prompt_unsaved_changes", path);
		return;
	}

	ApplicationManifest newManifest;
	ManifestValidationResult result;
	bool isDiaGameFile = false;
	DiaGameManifest gameManifest;

	// Detect .diagame extension — route to ComposeFromGameFile
	const char* ext = strrchr(path, '.');
	if (ext != nullptr && _stricmp(ext, ".diagame") == 0)
	{
		ManifestComposer composer;
		result = composer.ComposeFromGameFile(path, newManifest, gameManifest);
		isDiaGameFile = true;
	}
	else
	{
		ApplicationTypeRegistry registry;
		ApplicationManifestLoader loader(registry);
		result = loader.LoadFromFile(path, newManifest);
	}

	// kUnknownType is non-fatal in the editor — type registry is empty offline;
	// live type validation is deferred to Phase 3. Any other error means the file
	// could not be structurally parsed and should be rejected.
	if (result != ManifestValidationResult::kSuccess &&
		result != ManifestValidationResult::kUnknownType)
	{
		NotifyUI("file_error", "Failed to parse manifest");
		return;
	}

	if (newManifest.processingUnits.Size() == 0)
		NotifyUI("warning", "Manifest has no processing units — check imports");

	mData->manifest = newManifest;
	strncpy_s(mData->filePath, sizeof(mData->filePath), path, _TRUNCATE);
	mData->isDirty = false;
	mData->hasManifest = true;
	mData->isDiaGameFile = isDiaGameFile;
	mData->gameManifest = gameManifest;

	// Preserve raw .diagame config (may contain fields not in DiaGameConfig struct)
	delete mData->gameFileRawConfig;
	mData->gameFileRawConfig = nullptr;
	if (isDiaGameFile)
	{
		std::ifstream rawFile(path);
		if (rawFile.is_open())
		{
			Json::Value rawRoot;
			Json::Reader rawReader;
			if (rawReader.parse(rawFile, rawRoot) && rawRoot.isMember("config"))
				mData->gameFileRawConfig = new Json::Value(rawRoot["config"]);
			rawFile.close();
		}
	}

	// Serialize manifest to JSON and push to UI so TreeView can render it
	Json::Value manifestJson;
	ManifestSerializer::Serialize(mData->manifest, manifestJson);
	Json::Value loadedPayload;
	loadedPayload["manifest"] = manifestJson;
	loadedPayload["is_dirty"] = false;
	if (mBridge != nullptr)
		mBridge->NotifyUIDataChanged("manifest_loaded", loadedPayload);

	StartWatchingFile();
	ValidateManifest();
}

void DiaApplicationEditor::SaveManifest()
{
	if (!mData->hasManifest)
		return;

	if (mData->filePath[0] == '\0')
	{
		NotifyUI("show_save_dialog");
		return;
	}

	if (!ValidateManifest())
		return;

	mIsSaving = true;
	if (!WriteManifestToDisk(mData->filePath))
	{
		mIsSaving = false;
		return;
	}

	MarkClean();
	mIsSaving = false;
	// Silence the file-change notification that the write itself triggers by
	// re-recording the new modification time now.
	StartWatchingFile();
	// Dismiss any pending conflict banner.
	NotifyUI("conflict_resolved");
	NotifyUI("manifest_saved");
}

void DiaApplicationEditor::SaveManifestAs(const char* path)
{
	if (path == nullptr || path[0] == '\0')
	{
		NotifyUI("show_save_dialog");
		return;
	}

	strncpy_s(mData->filePath, sizeof(mData->filePath), path, _TRUNCATE);
	SaveManifest();
}

void DiaApplicationEditor::CloseManifest()
{
	if (IsDirty())
	{
		NotifyUI("prompt_unsaved_changes");
		return;
	}

	mData->manifest = ApplicationManifest();
	mData->filePath[0] = '\0';
	mData->isDirty = false;
	mData->hasManifest = false;
	mData->validationResult = ValidationResult();

	StopWatchingFile();
	NotifyUI("manifest_closed");
}

bool DiaApplicationEditor::IsDirty() const
{
	return mData != nullptr && mData->isDirty;
}

void DiaApplicationEditor::MarkDirty()
{
	if (mData != nullptr)
		mData->isDirty = true;
}

void DiaApplicationEditor::MarkClean()
{
	if (mData != nullptr)
		mData->isDirty = false;
}

bool DiaApplicationEditor::ValidateManifest()
{
	if (!mData->hasManifest)
		return false;

	// Seed registry from the manifest itself — every type the manifest references is by definition
	// a known type for this project. This lets the validator pass type checks offline without
	// needing a game-specific type list.
	ApplicationTypeRegistry registry;
	for (unsigned int i = 0; i < mData->manifest.processingUnits.Size(); ++i)
	{
		const ApplicationManifest::ProcessingUnitEntry& pu = mData->manifest.processingUnits[i];
		registry.RegisterKnownProcessingUnitType(pu.typeId);
		for (unsigned int j = 0; j < pu.phases.Size(); ++j)
			registry.RegisterKnownPhaseType(pu.phases[j].typeId);
		for (unsigned int j = 0; j < pu.modules.Size(); ++j)
			registry.RegisterKnownModuleType(pu.modules[j].typeId);
	}
	ManifestValidator validator(registry);

	mData->validationResult.errors.RemoveAll();
	ManifestValidationResult result = validator.Validate(mData->manifest);

	const auto& errors = validator.GetErrors();
	for (unsigned int i = 0; i < errors.Size(); ++i)
		mData->validationResult.errors.Add(errors[i]);

	// Offline: treat kUnknownType as non-fatal — structural validity is what matters here
	bool hasStructuralErrors = false;
	for (unsigned int i = 0; i < mData->validationResult.errors.Size(); ++i)
	{
		if (mData->validationResult.errors[i].code != ManifestValidationResult::kUnknownType)
		{
			hasStructuralErrors = true;
			break;
		}
	}
	mData->validationResult.isValid = !hasStructuralErrors;

	// Push full validation result to UI
	Json::Value payload;
	payload["is_valid"] = mData->validationResult.isValid;

	Json::Value errorsJson(Json::arrayValue);
	for (unsigned int i = 0; i < mData->validationResult.errors.Size(); ++i)
	{
		const ManifestValidationError& e = mData->validationResult.errors[i];
		// kUnknownType is expected offline — type registry is empty until Phase 3 live connection
		if (e.code == ManifestValidationResult::kUnknownType)
			continue;
		Json::Value entry;
		entry["message"]  = e.message.AsCStr();
		entry["context"]  = e.context.AsCStr();
		entry["severity"] = "error";
		entry["code"]     = ManifestValidationError::GetResultString(e.code);
		errorsJson.append(entry);
	}
	payload["errors"] = errorsJson;

	if (mBridge != nullptr)
		mBridge->NotifyUIDataChanged("validation_complete", payload);

	return mData->validationResult.isValid;
}

const ValidationResult& DiaApplicationEditor::GetValidationResult() const
{
	return mData->validationResult;
}

static void ReadImportsFromFile(const char* filePath, ApplicationManifest& outManifest)
{
	std::ifstream file(filePath);
	if (!file.is_open()) return;

	Json::Value root;
	Json::Reader reader;
	if (!reader.parse(file, root)) return;
	file.close();

	if (!root.isMember("imports") || !root["imports"].isArray()) return;

	const Json::Value& importsJson = root["imports"];
	for (unsigned int i = 0; i < importsJson.size(); ++i)
	{
		if (importsJson[i].isObject() && importsJson[i].isMember("path"))
		{
			TypedImport import;
			import.path = importsJson[i]["path"].asCString();
			const std::string typeStr = importsJson[i].get("type", "manifest").asString();
			import.type = (typeStr == "stage")
				? TypedImport::ImportType::kStage
				: TypedImport::ImportType::kManifest;
			outManifest.imports.Add(import);
		}
	}
}

static void BuildLocalManifest(const ApplicationManifest& merged, const char* localFilePath, ApplicationManifest& outLocal)
{
	outLocal.version = merged.version;

	// Preserve imports from the merged manifest (for single-file editing)
	for (unsigned int i = 0; i < merged.imports.Size(); ++i)
		outLocal.imports.Add(merged.imports[i]);

	auto isLocal = [localFilePath](const Dia::Core::Containers::String256& sourcePath) -> bool
	{
		if (sourcePath.Length() == 0) return true;
		if (localFilePath == nullptr || localFilePath[0] == '\0') return true;
		return _stricmp(sourcePath.AsCStr(), localFilePath) == 0;
	};

	for (unsigned int i = 0; i < merged.processingUnits.Size(); ++i)
	{
		const auto& pu = merged.processingUnits[i];
		if (!isLocal(pu.sourceManifestPath))
			continue;

		outLocal.processingUnits.AddDefault();
		auto& outPU = outLocal.processingUnits[outLocal.processingUnits.Size() - 1];
		outPU.typeId = pu.typeId;
		outPU.instanceId = pu.instanceId;
		outPU.frequencyHz = pu.frequencyHz;
		outPU.dedicatedThread = pu.dedicatedThread;
		outPU.root = pu.root;
		outPU.initialPhase = pu.initialPhase;
		if (pu.config)
			outPU.config = new Json::Value(*pu.config);

		// Only include local phases
		for (unsigned int j = 0; j < pu.phases.Size(); ++j)
		{
			if (isLocal(pu.phases[j].sourceManifestPath))
				outPU.phases.Add(pu.phases[j]);
		}

		// Only include local transitions
		for (unsigned int j = 0; j < pu.transitions.Size(); ++j)
		{
			if (isLocal(pu.transitions[j].sourceManifestPath))
				outPU.transitions.Add(pu.transitions[j]);
		}

		// Only include local modules, with only local phase_ids
		for (unsigned int j = 0; j < pu.modules.Size(); ++j)
		{
			const auto& mod = pu.modules[j];
			if (!isLocal(mod.sourceManifestPath))
				continue;

			ApplicationManifest::ModuleEntry outMod;
			outMod.typeId = mod.typeId;
			outMod.instanceId = mod.instanceId;
			if (mod.config)
				outMod.config = new Json::Value(*mod.config);

			for (unsigned int k = 0; k < mod.dependencies.Size(); ++k)
				outMod.dependencies.Add(mod.dependencies[k]);

			// Only include phase_ids that reference phases local to this file
			for (unsigned int k = 0; k < mod.phaseIds.Size(); ++k)
			{
				bool phaseIsLocal = false;
				for (unsigned int p = 0; p < pu.phases.Size(); ++p)
				{
					if (pu.phases[p].instanceId == mod.phaseIds[k] && isLocal(pu.phases[p].sourceManifestPath))
					{
						phaseIsLocal = true;
						break;
					}
				}
				if (phaseIsLocal)
					outMod.phaseIds.Add(mod.phaseIds[k]);
			}

			outPU.modules.Add(outMod);
		}
	}
}

bool DiaApplicationEditor::WriteManifestToDisk(const char* path)
{
	if (mData->isDiaGameFile)
		return WriteDiaGameToDisk(path);

	// Create .bak backup if file already exists
	std::ifstream existingFile(path);
	if (existingFile.good())
	{
		existingFile.close();
		char backupPath[512 + 4];
		snprintf(backupPath, sizeof(backupPath), "%s.bak", path);
		std::rename(path, backupPath);
	}

	// Build a manifest containing only entries local to this file
	ApplicationManifest localManifest;
	BuildLocalManifest(mData->manifest, mData->filePath, localManifest);

	// Use the shared serializer (same as runtime) to write to disk
	JsonApplicationManifestSerializer serializer;
	auto result = serializer.SaveToFile(path, localManifest);
	if (!result)
	{
		NotifyUI("file_error", "Failed to write manifest to disk");
		return false;
	}

	return true;
}

static bool IsStageSourceFile(const ApplicationManifest& manifest, const char* sourcePath)
{
	// A source file is a stage if it contributes phases/modules/transitions to PUs
	// it doesn't own (i.e., no PU has this as its sourceManifestPath)
	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		if (_stricmp(manifest.processingUnits[i].sourceManifestPath.AsCStr(), sourcePath) == 0)
			return false;
	}
	return true;
}

static void BuildStageJson(const ApplicationManifest& manifest, const char* stageSourcePath, const Json::Value& originalMetadata, Json::Value& outJson)
{
	outJson = Json::Value(Json::objectValue);
	outJson["version"] = manifest.version;
	outJson["metadata"] = originalMetadata;

	Json::Value stagePhases(Json::arrayValue);
	Json::Value stageTransitions(Json::arrayValue);
	Json::Value stageModules(Json::arrayValue);

	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		const auto& pu = manifest.processingUnits[i];
		const char* puInstanceId = pu.instanceId.AsChar();

		// Collect phases from this stage
		for (unsigned int j = 0; j < pu.phases.Size(); ++j)
		{
			if (_stricmp(pu.phases[j].sourceManifestPath.AsCStr(), stageSourcePath) == 0)
			{
				Json::Value sp;
				sp["type_id"] = pu.phases[j].typeId.AsChar();
				sp["instance_id"] = pu.phases[j].instanceId.AsChar();
				sp["target_processing_unit"] = puInstanceId;
				if (pu.phases[j].config)
					sp["config"] = *pu.phases[j].config;
				stagePhases.append(sp);
			}
		}

		// Collect transitions from this stage
		for (unsigned int j = 0; j < pu.transitions.Size(); ++j)
		{
			if (_stricmp(pu.transitions[j].sourceManifestPath.AsCStr(), stageSourcePath) == 0)
			{
				Json::Value st;
				st["from"] = pu.transitions[j].fromPhase.AsChar();
				st["to"] = pu.transitions[j].toPhase.AsChar();
				st["target_processing_unit"] = puInstanceId;
				stageTransitions.append(st);
			}
		}

		// Collect stage module contributions — modules that have phase_ids referencing stage phases
		for (unsigned int j = 0; j < pu.modules.Size(); ++j)
		{
			const auto& mod = pu.modules[j];

			// Collect phase_ids that belong to this stage (phases sourced from stageSourcePath)
			Json::Value stagePhaseIds(Json::arrayValue);
			for (unsigned int k = 0; k < mod.phaseIds.Size(); ++k)
			{
				// Check if this phase_id references a phase from the stage
				for (unsigned int p = 0; p < pu.phases.Size(); ++p)
				{
					if (pu.phases[p].instanceId == mod.phaseIds[k] &&
						_stricmp(pu.phases[p].sourceManifestPath.AsCStr(), stageSourcePath) == 0)
					{
						stagePhaseIds.append(mod.phaseIds[k].AsChar());
						break;
					}
				}
			}

			if (stagePhaseIds.size() > 0)
			{
				Json::Value sm;
				sm["type_id"] = mod.typeId.AsChar();
				sm["instance_id"] = mod.instanceId.AsChar();
				sm["phase_ids"] = stagePhaseIds;
				sm["target_processing_unit"] = puInstanceId;
				stageModules.append(sm);
			}
		}
	}

	outJson["stage_phases"] = stagePhases;
	outJson["stage_transitions"] = stageTransitions;
	outJson["stage_modules"] = stageModules;
	outJson["processing_units"] = Json::Value(Json::arrayValue);
}

bool DiaApplicationEditor::WriteDiaGameToDisk(const char* path)
{
	// For .diagame files, write changes back to the source .diaapp files
	// Collect all unique source paths from PUs, phases, modules, and transitions

	struct SourceGroup
	{
		char path[512];
		bool isStage;
	};
	Dia::Core::Containers::DynamicArrayC<SourceGroup, 8> groups;

	auto addSource = [&groups](const char* sourcePath)
	{
		if (sourcePath == nullptr || sourcePath[0] == '\0')
			return;
		for (unsigned int g = 0; g < groups.Size(); ++g)
		{
			if (_stricmp(groups[g].path, sourcePath) == 0)
				return;
		}
		groups.AddDefault();
		SourceGroup& newGroup = groups[groups.Size() - 1];
		strncpy_s(newGroup.path, sizeof(newGroup.path), sourcePath, _TRUNCATE);
		newGroup.isStage = false;
	};

	// Collect source paths from all entries
	for (unsigned int i = 0; i < mData->manifest.processingUnits.Size(); ++i)
	{
		const auto& pu = mData->manifest.processingUnits[i];
		addSource(pu.sourceManifestPath.AsCStr());
		for (unsigned int j = 0; j < pu.phases.Size(); ++j)
			addSource(pu.phases[j].sourceManifestPath.AsCStr());
		for (unsigned int j = 0; j < pu.modules.Size(); ++j)
			addSource(pu.modules[j].sourceManifestPath.AsCStr());
		for (unsigned int j = 0; j < pu.transitions.Size(); ++j)
			addSource(pu.transitions[j].sourceManifestPath.AsCStr());
	}

	// Classify each group as stage or regular manifest
	for (unsigned int g = 0; g < groups.Size(); ++g)
		groups[g].isStage = IsStageSourceFile(mData->manifest, groups[g].path);

	// Write each source file
	JsonApplicationManifestSerializer serializer;
	Json::StyledWriter writer;
	for (unsigned int g = 0; g < groups.Size(); ++g)
	{
		const SourceGroup& group = groups[g];

		// Read data from original file before backup renames it
		Json::Value originalMetadata(Json::objectValue);
		ApplicationManifest originalImports;
		if (group.isStage)
		{
			originalMetadata["type"] = "stage";
			std::ifstream origFile(group.path);
			if (origFile.is_open())
			{
				Json::Value origRoot;
				Json::Reader reader;
				if (reader.parse(origFile, origRoot) && origRoot.isMember("metadata"))
					originalMetadata = origRoot["metadata"];
				origFile.close();
			}
		}
		else
		{
			ReadImportsFromFile(group.path, originalImports);
		}

		// Back up the source file
		std::ifstream existingFile(group.path);
		if (existingFile.good())
		{
			existingFile.close();
			char backupPath[512 + 4];
			snprintf(backupPath, sizeof(backupPath), "%s.bak", group.path);
			std::rename(group.path, backupPath);
		}

		if (group.isStage)
		{
			// Reconstruct stage file format
			Json::Value stageJson;
			BuildStageJson(mData->manifest, group.path, originalMetadata, stageJson);

			std::ofstream file(group.path);
			if (!file.is_open())
			{
				char errMsg[600];
				snprintf(errMsg, sizeof(errMsg), "Cannot write to: %s", group.path);
				NotifyUI("file_error", errMsg);
				return false;
			}
			file << writer.write(stageJson);
			file.close();
		}
		else
		{
			// Regular manifest — build local-only and use shared serializer
			ApplicationManifest localManifest;
			BuildLocalManifest(mData->manifest, group.path, localManifest);

			// Restore original imports (lost during composition)
			if (localManifest.imports.Size() == 0 && originalImports.imports.Size() > 0)
			{
				for (unsigned int ii = 0; ii < originalImports.imports.Size(); ++ii)
					localManifest.imports.Add(originalImports.imports[ii]);
			}

			auto result = serializer.SaveToFile(group.path, localManifest);
			if (!result)
			{
				char errMsg[600];
				snprintf(errMsg, sizeof(errMsg), "Cannot write to: %s", group.path);
				NotifyUI("file_error", errMsg);
				return false;
			}
		}
	}

	// Now write the .diagame file itself (preserving its schema)
	std::ifstream existingGameFile(path);
	if (existingGameFile.good())
	{
		existingGameFile.close();
		char backupPath[512 + 4];
		snprintf(backupPath, sizeof(backupPath), "%s.bak", path);
		std::rename(path, backupPath);
	}

	Json::Value gameJson(Json::objectValue);
	gameJson["name"] = mData->gameManifest.name.AsCStr();
	gameJson["version"] = mData->gameManifest.version.AsCStr();

	Json::Value importsJson(Json::arrayValue);
	for (unsigned int i = 0; i < mData->gameManifest.imports.Size(); ++i)
	{
		Json::Value entry;
		entry["path"] = mData->gameManifest.imports[i].path.AsCStr();
		entry["type"] = (mData->gameManifest.imports[i].type == TypedImport::ImportType::kStage)
			? "stage" : "manifest";
		importsJson.append(entry);
	}
	gameJson["imports"] = importsJson;

	if (mData->gameFileRawConfig != nullptr)
		gameJson["config"] = *mData->gameFileRawConfig;
	else
	{
		Json::Value configJson(Json::objectValue);
		if (mData->gameManifest.config.assetRoot.Length() > 0)
			configJson["asset_root"] = mData->gameManifest.config.assetRoot.AsCStr();
		gameJson["config"] = configJson;
	}

	std::ofstream gameFile(path);
	if (!gameFile.is_open())
	{
		NotifyUI("file_error", "Cannot open .diagame file for writing");
		return false;
	}

	gameFile << writer.write(gameJson);
	gameFile.close();

	return true;
}

void DiaApplicationEditor::NotifyUI(const char* topic, const char* message)
{
	if (mBridge == nullptr)
		return;

	Json::Value data;
	if (message != nullptr)
		data["message"] = message;

	mBridge->NotifyUIDataChanged(topic, data);
}

void DiaApplicationEditor::NotifyManifestUpdated()
{
	if (mBridge == nullptr || !mData->hasManifest)
		return;

	Json::Value manifestJson;
	ManifestSerializer::Serialize(mData->manifest, manifestJson);
	Json::Value payload;
	payload["manifest"] = manifestJson;
	payload["is_dirty"] = mData->isDirty;
	mBridge->NotifyUIDataChanged("manifest_updated", payload);
}

void DiaApplicationEditor::RefreshAvailableTypes()
{
	// Phase 3 will add live game registry query here.
	// For now always load from the bundled static list.
	LoadStaticTypeList();
}

const TypeCache& DiaApplicationEditor::GetTypeCache() const
{
	return mData->typeCache;
}

void DiaApplicationEditor::LoadStaticTypeList()
{
	mData->typeCache.Clear();

	std::ifstream file("plugins/diaapplicationeditor/known_types.json");
	if (!file.is_open())
	{
		NotifyUI("type_discovery_error", "Could not open known_types.json");
		return;
	}

	Json::Value root;
	Json::Reader reader;
	if (!reader.parse(file, root))
	{
		NotifyUI("type_discovery_error", "Failed to parse known_types.json");
		return;
	}

	auto parseInto = [](const Json::Value& arr, auto& outArray)
	{
		for (unsigned int i = 0; i < (unsigned int)arr.size(); ++i)
		{
			const Json::Value& entry = arr[i];
			if (!entry.isMember("type"))
				continue;

			TypeInfo info;
			info.typeId = Dia::Core::StringCRC(entry["type"].asCString());
			if (entry.isMember("description"))
				strncpy_s(info.description, sizeof(info.description), entry["description"].asCString(), _TRUNCATE);

			outArray.Add(info);
		}
	};

	if (root.isMember("processing_units"))
		parseInto(root["processing_units"], mData->typeCache.processingUnitTypes);
	if (root.isMember("phases"))
		parseInto(root["phases"], mData->typeCache.phaseTypes);
	if (root.isMember("modules"))
		parseInto(root["modules"], mData->typeCache.moduleTypes);

	mData->typeCache.isFromLiveGame = false;

	NotifyUI("types_refreshed");
}

// Tree node helpers

int DiaApplicationEditor::FindProcessingUnitIndex(const char* puId) const
{
	Dia::Core::StringCRC id(puId);
	for (unsigned int i = 0; i < mData->manifest.processingUnits.Size(); ++i)
	{
		if (mData->manifest.processingUnits[i].instanceId == id)
			return static_cast<int>(i);
	}
	return -1;
}

int DiaApplicationEditor::FindPhaseIndex(int puIndex, const char* phaseId) const
{
	if (puIndex < 0) return -1;
	const auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIndex)];
	Dia::Core::StringCRC id(phaseId);
	for (unsigned int i = 0; i < pu.phases.Size(); ++i)
	{
		if (pu.phases[i].instanceId == id)
			return static_cast<int>(i);
	}
	return -1;
}

int DiaApplicationEditor::FindModuleIndex(int puIndex, const char* moduleId) const
{
	if (puIndex < 0) return -1;
	const auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIndex)];
	Dia::Core::StringCRC id(moduleId);
	for (unsigned int i = 0; i < pu.modules.Size(); ++i)
	{
		if (pu.modules[i].instanceId == id)
			return static_cast<int>(i);
	}
	return -1;
}

void DiaApplicationEditor::HandleAddNode(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("parent_id") || !data.isMember("node_type")) return;

	const std::string parentId = data["parent_id"].asString();
	const std::string nodeType = data["node_type"].asString();

	Json::Value payload;
	payload["node_type"] = nodeType;

	if (nodeType == "phase")
	{
		// parent_id is a PU instance_id — resolve it to validate
		int puIdx = FindProcessingUnitIndex(parentId.c_str());
		if (puIdx < 0) return;
		payload["pu_id"] = parentId;
	}
	else if (nodeType == "module")
	{
		// parent_id is composite "puId_phaseId" — resolve by iterating PUs/phases
		bool resolved = false;
		for (unsigned int i = 0; i < mData->manifest.processingUnits.Size(); ++i)
		{
			const auto& pu = mData->manifest.processingUnits[i];
			const std::string puId(pu.instanceId.AsChar());
			for (unsigned int j = 0; j < pu.phases.Size(); ++j)
			{
				const std::string phaseId(pu.phases[j].instanceId.AsChar());
				const std::string composite = puId + "_" + phaseId;
				if (composite == parentId)
				{
					payload["pu_id"] = puId;
					payload["phase_id"] = phaseId;
					resolved = true;
					break;
				}
			}
			if (resolved) break;
		}
		if (!resolved) return;
	}

	Json::Value availableTypes(Json::arrayValue);
	if (nodeType == "phase")
	{
		for (unsigned int i = 0; i < mData->typeCache.phaseTypes.Size(); ++i)
		{
			Json::Value entry;
			entry["type"] = mData->typeCache.phaseTypes[i].typeId.AsChar();
			entry["description"] = mData->typeCache.phaseTypes[i].description;
			availableTypes.append(entry);
		}
	}
	else if (nodeType == "module")
	{
		for (unsigned int i = 0; i < mData->typeCache.moduleTypes.Size(); ++i)
		{
			Json::Value entry;
			entry["type"] = mData->typeCache.moduleTypes[i].typeId.AsChar();
			entry["description"] = mData->typeCache.moduleTypes[i].description;
			availableTypes.append(entry);
		}
	}
	payload["available_types"] = availableTypes;

	if (mBridge != nullptr)
		mBridge->NotifyUIDataChanged("show_type_selector", payload);
}

void DiaApplicationEditor::HandleAddNodeConfirmed(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("pu_id") || !data.isMember("node_type") ||
		!data.isMember("type_name") || !data.isMember("instance_id"))
		return;

	const std::string puId = data["pu_id"].asString();
	const std::string nodeType = data["node_type"].asString();
	const std::string typeName = data["type_name"].asString();
	const std::string instanceId = data["instance_id"].asString();

	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;

	auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)];

	if (nodeType == "phase")
	{
		Dia::Core::StringCRC newId(instanceId.c_str());
		for (unsigned int i = 0; i < pu.phases.Size(); ++i)
		{
			if (pu.phases[i].instanceId == newId)
			{
				NotifyUI("add_node_error", "A phase with this instance ID already exists");
				return;
			}
		}

		ApplicationManifest::PhaseEntry phase;
		phase.typeId = Dia::Core::StringCRC(typeName.c_str());
		phase.instanceId = newId;
		pu.phases.Add(phase);

		MarkDirty();
		NotifyManifestUpdated();
	}
	else if (nodeType == "module")
	{
		if (!data.isMember("phase_id")) return;
		const std::string phaseId = data["phase_id"].asString();

		Dia::Core::StringCRC newId(instanceId.c_str());
		for (unsigned int i = 0; i < pu.modules.Size(); ++i)
		{
			if (pu.modules[i].instanceId == newId)
			{
				NotifyUI("add_node_error", "A module with this instance ID already exists");
				return;
			}
		}

		ApplicationManifest::ModuleEntry module;
		module.typeId = Dia::Core::StringCRC(typeName.c_str());
		module.instanceId = newId;
		module.phaseIds.Add(Dia::Core::StringCRC(phaseId.c_str()));
		pu.modules.Add(module);

		MarkDirty();
		NotifyManifestUpdated();
	}
}

void DiaApplicationEditor::HandleRemoveNode(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("node_id")) return;

	const std::string nodeId = data["node_id"].asString();

	// Node ID format: "puId" | "puId_phaseId" | "puId_phaseId_moduleId"
	// Split on first two underscores
	const size_t sep1 = nodeId.find('_');
	if (sep1 == std::string::npos)
	{
		// Processing unit
		int puIdx = FindProcessingUnitIndex(nodeId.c_str());
		if (puIdx >= 0)
		{
			mData->manifest.processingUnits.RemoveAt(static_cast<unsigned int>(puIdx));
			MarkDirty();
			NotifyManifestUpdated();
		}
		return;
	}

	const std::string puId = nodeId.substr(0, sep1);
	const size_t sep2 = nodeId.find('_', sep1 + 1);
	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;

	auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)];

	if (sep2 == std::string::npos)
	{
		// Phase
		const std::string phaseId = nodeId.substr(sep1 + 1);
		int phaseIdx = FindPhaseIndex(puIdx, phaseId.c_str());
		if (phaseIdx >= 0)
		{
			pu.phases.RemoveAt(static_cast<unsigned int>(phaseIdx));
			MarkDirty();
			NotifyManifestUpdated();
		}
	}
	else
	{
		// Module
		const std::string moduleId = nodeId.substr(sep2 + 1);
		int moduleIdx = FindModuleIndex(puIdx, moduleId.c_str());
		if (moduleIdx >= 0)
		{
			pu.modules.RemoveAt(static_cast<unsigned int>(moduleIdx));
			MarkDirty();
			NotifyManifestUpdated();
		}
	}
}

void DiaApplicationEditor::HandleReorderNode(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("node_id") || !data.isMember("new_index")) return;

	const std::string nodeId = data["node_id"].asString();
	const int newIndex = data["new_index"].asInt();
	if (newIndex < 0) return;

	const size_t sep1 = nodeId.find('_');
	if (sep1 == std::string::npos) return; // PU reorder not supported; PUs don't move

	const std::string puId = nodeId.substr(0, sep1);
	const size_t sep2 = nodeId.find('_', sep1 + 1);
	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;

	auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)];

	if (sep2 == std::string::npos)
	{
		// Reorder phase
		const std::string phaseId = nodeId.substr(sep1 + 1);
		int oldIdx = FindPhaseIndex(puIdx, phaseId.c_str());
		if (oldIdx < 0 || oldIdx == newIndex) return;
		if (static_cast<unsigned int>(newIndex) >= pu.phases.Size()) return;

		ApplicationManifest::PhaseEntry entry = pu.phases[static_cast<unsigned int>(oldIdx)];
		pu.phases.RemoveAt(static_cast<unsigned int>(oldIdx));
		pu.phases.AddAt(entry, static_cast<unsigned int>(newIndex));
		MarkDirty();
		NotifyManifestUpdated();
	}
	else
	{
		// Reorder module
		const std::string moduleId = nodeId.substr(sep2 + 1);
		int oldIdx = FindModuleIndex(puIdx, moduleId.c_str());
		if (oldIdx < 0 || oldIdx == newIndex) return;
		if (static_cast<unsigned int>(newIndex) >= pu.modules.Size()) return;

		ApplicationManifest::ModuleEntry entry = pu.modules[static_cast<unsigned int>(oldIdx)];
		pu.modules.RemoveAt(static_cast<unsigned int>(oldIdx));
		pu.modules.AddAt(entry, static_cast<unsigned int>(newIndex));
		MarkDirty();
		NotifyManifestUpdated();
	}
}

void DiaApplicationEditor::HandlePhasePositionsChanged(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("nodes") || !data["nodes"].isArray()) return;

	bool changed = false;
	const Json::Value& nodeArray = data["nodes"];
	for (Json::ArrayIndex i = 0; i < nodeArray.size(); ++i)
	{
		const Json::Value& node = nodeArray[i];
		if (!node.isMember("id") || !node.isMember("position")) continue;

		const std::string nodeId = node["id"].asString();
		const float x = node["position"].isMember("x") ? node["position"]["x"].asFloat() : 0.f;
		const float y = node["position"].isMember("y") ? node["position"]["y"].asFloat() : 0.f;

		// Node ID is "puId_phaseId" — split at first underscore
		const size_t sep = nodeId.find('_');
		if (sep == std::string::npos) continue;

		const std::string puId    = nodeId.substr(0, sep);
		const std::string phaseId = nodeId.substr(sep + 1);

		int puIdx = FindProcessingUnitIndex(puId.c_str());
		if (puIdx < 0) continue;
		int phaseIdx = FindPhaseIndex(puIdx, phaseId.c_str());
		if (phaseIdx < 0) continue;

		auto& phase = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)]
		                            .phases[static_cast<unsigned int>(phaseIdx)];

		if (phase.config == nullptr)
			phase.config = new Json::Value(Json::objectValue);

		(*phase.config)["editor_position"]["x"] = x;
		(*phase.config)["editor_position"]["y"] = y;
		changed = true;
	}

	if (changed)
		MarkDirty();
}

// Transition helpers: node IDs are "puId_phaseId"; parse both sides sharing the same PU prefix.
// Returns puIdx, fromPhaseIdx, toPhaseIdx via out-params. Returns false on parse/lookup failure.
static bool ParseTransitionIds(
	const std::string& fromNodeId,
	const std::string& toNodeId,
	const Dia::Application::ApplicationManifest& manifest,
	int& outPuIdx, int& outFromPhaseIdx, int& outToPhaseIdx)
{
	// Both node IDs share the same puId prefix — use fromNodeId to determine it.
	const size_t sep = fromNodeId.find('_');
	if (sep == std::string::npos) return false;

	const std::string puId       = fromNodeId.substr(0, sep);
	const std::string fromPhaseId = fromNodeId.substr(sep + 1);

	// toNodeId may use a different prefix if the user somehow dragged across PUs,
	// but we only support same-PU transitions, so just strip the first segment.
	const size_t sep2 = toNodeId.find('_');
	const std::string toPhaseId = (sep2 != std::string::npos) ? toNodeId.substr(sep2 + 1) : toNodeId;

	const Dia::Core::StringCRC puCRC(puId.c_str());
	for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
	{
		if (manifest.processingUnits[i].instanceId == puCRC)
		{
			outPuIdx = static_cast<int>(i);
			const auto& pu = manifest.processingUnits[i];
			const Dia::Core::StringCRC fromCRC(fromPhaseId.c_str());
			const Dia::Core::StringCRC toCRC(toPhaseId.c_str());
			for (unsigned int j = 0; j < pu.phases.Size(); ++j)
				if (pu.phases[j].instanceId == fromCRC) outFromPhaseIdx = static_cast<int>(j);
			for (unsigned int j = 0; j < pu.phases.Size(); ++j)
				if (pu.phases[j].instanceId == toCRC)  outToPhaseIdx   = static_cast<int>(j);
			return outFromPhaseIdx >= 0 && outToPhaseIdx >= 0;
		}
	}
	return false;
}

void DiaApplicationEditor::HandleTransitionAdded(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("from_phase") || !data.isMember("to_phase")) return;

	const std::string fromId = data["from_phase"].asString();
	const std::string toId   = data["to_phase"].asString();

	int puIdx = -1, fromIdx = -1, toIdx = -1;
	if (!ParseTransitionIds(fromId, toId, mData->manifest, puIdx, fromIdx, toIdx)) return;

	auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)];
	const Dia::Core::StringCRC fromCRC = pu.phases[static_cast<unsigned int>(fromIdx)].instanceId;
	const Dia::Core::StringCRC toCRC   = pu.phases[static_cast<unsigned int>(toIdx)].instanceId;

	// Check for duplicates
	for (unsigned int i = 0; i < pu.transitions.Size(); ++i)
	{
		if (pu.transitions[i].fromPhase == fromCRC && pu.transitions[i].toPhase == toCRC)
			return;
	}

	ApplicationManifest::PhaseTransition t;
	t.fromPhase = fromCRC;
	t.toPhase   = toCRC;
	pu.transitions.Add(t);

	MarkDirty();
}

void DiaApplicationEditor::HandleTransitionRemoved(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("from_phase") || !data.isMember("to_phase")) return;

	const std::string fromId = data["from_phase"].asString();
	const std::string toId   = data["to_phase"].asString();

	int puIdx = -1, fromIdx = -1, toIdx = -1;
	if (!ParseTransitionIds(fromId, toId, mData->manifest, puIdx, fromIdx, toIdx)) return;

	auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)];
	const Dia::Core::StringCRC fromCRC = pu.phases[static_cast<unsigned int>(fromIdx)].instanceId;
	const Dia::Core::StringCRC toCRC   = pu.phases[static_cast<unsigned int>(toIdx)].instanceId;

	for (unsigned int i = 0; i < pu.transitions.Size(); ++i)
	{
		if (pu.transitions[i].fromPhase == fromCRC && pu.transitions[i].toPhase == toCRC)
		{
			pu.transitions.RemoveAt(i);
			MarkDirty();
			return;
		}
	}
}

void DiaApplicationEditor::HandleTransitionConditionChanged(const Json::Value& data)
{
	// ApplicationManifest::PhaseTransition has no condition field — transitions are just
	// from/to pairs.  Condition labels are stored in the UI only (no C++ persistence needed).
	// This handler marks dirty so the manifest is re-saved, preserving future metadata.
	(void)data;
	if (!mData->hasManifest) return;
	MarkDirty();
}

void DiaApplicationEditor::StartWatchingFile()
{
	if (!mData->hasManifest || mData->filePath[0] == '\0') return;

	mFileWatcher.Unwatch(mData->filePath);
	mFileWatcher.Watch(mData->filePath, [this](const char* /*path*/, Dia::Core::FileWatchEvent event) {
		if (event != Dia::Core::FileWatchEvent::Modified) return;
		if (mIsSaving) return;

		if (!mData->isDirty)
		{
			// No local changes — silently auto-reload
			ApplicationTypeRegistry registry;
			ApplicationManifestLoader loader(registry);
			ApplicationManifest fresh;
			if (loader.LoadFromFile(mData->filePath, fresh) == ManifestValidationResult::kSuccess)
			{
				mData->manifest = fresh;
				mData->isDirty = false;

				Json::Value manifestJson;
				ManifestSerializer::Serialize(mData->manifest, manifestJson);
				Json::Value payload;
				payload["manifest"] = manifestJson;
				payload["is_dirty"] = false;
				if (mBridge != nullptr)
					mBridge->NotifyUIDataChanged("manifest_loaded", payload);

				ValidateManifest();
			}
		}
		else
		{
			// Local changes exist — show conflict banner
			ApplicationTypeRegistry registry;
			ApplicationManifestLoader loader(registry);
			ApplicationManifest diskManifest;
			if (loader.LoadFromFile(mData->filePath, diskManifest) != ManifestValidationResult::kSuccess)
				return;

			Json::Value diskJson;
			ManifestSerializer::Serialize(diskManifest, diskJson);
			Json::Value localJson;
			ManifestSerializer::Serialize(mData->manifest, localJson);

			Json::Value payload;
			payload["disk_version"]  = diskJson;
			payload["local_version"] = localJson;
			if (mBridge != nullptr)
				mBridge->NotifyUIDataChanged("file_conflict_detected", payload);
		}
	});
}

void DiaApplicationEditor::StopWatchingFile()
{
	if (mData && mData->filePath[0] != '\0')
		mFileWatcher.Unwatch(mData->filePath);
}

void DiaApplicationEditor::HandleResolveConflict(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	const std::string action = data.isMember("action") ? data["action"].asString() : "";

	if (action == "reload_disk")
	{
		// Discard local changes and reload from disk — clear dirty flag first to
		// avoid the "prompt unsaved changes" guard in OpenManifest.
		mData->isDirty = false;
		OpenManifest(mData->filePath);
	}

	// For "keep_local" or empty action: just dismiss the banner.
	NotifyUI("conflict_resolved");
}

void DiaApplicationEditor::HandleModuleConfigChanged(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("processing_unit") || !data.isMember("module_id") || !data.isMember("config")) return;

	const std::string puId     = data["processing_unit"].asString();
	const std::string moduleId = data["module_id"].asString();
	const Json::Value& newConfig = data["config"];

	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;
	int modIdx = FindModuleIndex(puIdx, moduleId.c_str());
	if (modIdx < 0) return;

	auto& mod = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)]
	                            .modules[static_cast<unsigned int>(modIdx)];

	if (mod.config != nullptr)
		*mod.config = newConfig;
	else
		mod.config = new Json::Value(newConfig);

	MarkDirty();
	ValidateManifest();
}

void DiaApplicationEditor::HandlePUPropertyChanged(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("pu_id") || !data.isMember("property") || !data.isMember("value")) return;

	const std::string puId    = data["pu_id"].asString();
	const std::string prop    = data["property"].asString();
	const Json::Value& value  = data["value"];

	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;

	auto& pu = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)];

	if (prop == "instance_id" && value.isString())
		pu.instanceId = Dia::Core::StringCRC(value.asCString());
	else if (prop == "frequency_hz" && value.isDouble())
		pu.frequencyHz = value.asFloat();
	else if (prop == "dedicated_thread" && value.isBool())
		pu.dedicatedThread = value.asBool();
	else if (prop == "config" && value.isObject())
	{
		if (pu.config != nullptr)
			*pu.config = value;
		else
			pu.config = new Json::Value(value);
	}

	MarkDirty();
	NotifyManifestUpdated();
}

void DiaApplicationEditor::HandlePhasePropertyChanged(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("pu_id") || !data.isMember("phase_id") || !data.isMember("property") || !data.isMember("value")) return;

	const std::string puId    = data["pu_id"].asString();
	const std::string phaseId = data["phase_id"].asString();
	const std::string prop    = data["property"].asString();
	const Json::Value& value  = data["value"];

	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;
	int phaseIdx = FindPhaseIndex(puIdx, phaseId.c_str());
	if (phaseIdx < 0) return;

	auto& phase = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)]
	                              .phases[static_cast<unsigned int>(phaseIdx)];

	if (prop == "config" && value.isObject())
	{
		if (phase.config != nullptr)
			*phase.config = value;
		else
			phase.config = new Json::Value(value);
	}

	MarkDirty();
	NotifyManifestUpdated();
}

void DiaApplicationEditor::HandleInitialPhaseChanged(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("pu_id") || !data.isMember("phase_id")) return;

	const std::string puId    = data["pu_id"].asString();
	const std::string phaseId = data["phase_id"].asString();

	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;

	mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)].initialPhase =
		Dia::Core::StringCRC(phaseId.c_str());

	MarkDirty();
	NotifyManifestUpdated();
}

void DiaApplicationEditor::HandleModulePhasesChanged(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("pu_id") || !data.isMember("module_id") || !data.isMember("phase_ids")) return;

	const std::string puId     = data["pu_id"].asString();
	const std::string moduleId = data["module_id"].asString();
	const Json::Value& phaseIds = data["phase_ids"];
	if (!phaseIds.isArray()) return;

	int puIdx = FindProcessingUnitIndex(puId.c_str());
	if (puIdx < 0) return;
	int modIdx = FindModuleIndex(puIdx, moduleId.c_str());
	if (modIdx < 0) return;

	auto& mod = mData->manifest.processingUnits[static_cast<unsigned int>(puIdx)]
	                            .modules[static_cast<unsigned int>(modIdx)];

	mod.phaseIds.RemoveAll();
	for (Json::ArrayIndex i = 0; i < phaseIds.size(); ++i)
	{
		if (phaseIds[i].isString())
			mod.phaseIds.Add(Dia::Core::StringCRC(phaseIds[i].asCString()));
	}

	MarkDirty();
	NotifyManifestUpdated();
	ValidateManifest();
}

void DiaApplicationEditor::HandleNewManifest()
{
	StopWatchingFile();

	mData->manifest = ApplicationManifest();
	mData->manifest.version = 1;

	ApplicationManifest::ProcessingUnitEntry pu;
	pu.typeId = Dia::Core::StringCRC("NewProcessingUnit");
	pu.instanceId = Dia::Core::StringCRC("NewProcessingUnit");
	pu.frequencyHz = 30.0f;
	pu.dedicatedThread = false;
	pu.root = true;

	ApplicationManifest::PhaseEntry phase;
	phase.typeId = Dia::Core::StringCRC("InitPhase");
	phase.instanceId = Dia::Core::StringCRC("InitPhase");
	pu.phases.Add(phase);
	pu.initialPhase = Dia::Core::StringCRC("InitPhase");

	mData->manifest.processingUnits.Add(pu);

	mData->filePath[0] = '\0';
	mData->isDirty = true;
	mData->hasManifest = true;
}

void DiaApplicationEditor::HandleManifestRestored(const Json::Value& data)
{
	if (!data.isMember("manifest")) return;

	// Re-parse the manifest JSON from the UI's undo snapshot
	ApplicationTypeRegistry registry;
	ApplicationManifestLoader loader(registry);
	Json::FastWriter writer;
	std::string jsonStr = writer.write(data["manifest"]);

	ApplicationManifest restored;
	ManifestValidationResult result = loader.LoadFromString(jsonStr.c_str(), restored);
	if (result != ManifestValidationResult::kSuccess && result != ManifestValidationResult::kUnknownType)
		return;

	mData->manifest = restored;
	mData->hasManifest = true;
	NotifyManifestUpdated();
}

void DiaApplicationEditor::HandleAddImport(const Json::Value& data)
{
	if (!mData->hasManifest) return;

	char dialogPath[MAX_PATH] = {};
	const char* path = nullptr;

	if (data.isMember("path") && data["path"].isString())
	{
		path = data["path"].asCString();
	}
	else
	{
		Json::Value dialogData;
		Json::Value filters(Json::arrayValue);
		Json::Value f1; f1["name"] = "Dia App Manifest"; f1["ext"] = "*.diaapp"; filters.append(f1);
		Json::Value f2; f2["name"] = "All Files"; f2["ext"] = "*.*"; filters.append(f2);
		dialogData["filters"] = filters;
		dialogData["default_ext"] = "diaapp";
		dialogData["title"] = "Add Import";

		Json::Value result = Dia::Editor::FileDialogHandler::HandleOpenFileDialog(dialogData);
		if (!result.get("success", false).asBool())
			return;
		strncpy_s(dialogPath, MAX_PATH, result["path"].asCString(), _TRUNCATE);
		path = dialogPath;
	}

	if (path == nullptr || path[0] == '\0') return;

	// Check for duplicates
	for (unsigned int i = 0; i < mData->manifest.imports.Size(); ++i)
	{
		if (strcmp(mData->manifest.imports[i].path.AsCStr(), path) == 0)
		{
			NotifyUI("import_error", "This file is already imported");
			return;
		}
	}

	// Resolve the target file and all its transitive imports
	ManifestComposer composer;
	ApplicationManifest targetManifest;
	ManifestValidationResult result = composer.ComposeSingleManifest(path, targetManifest);
	if (result == ManifestValidationResult::kImportCycle)
	{
		NotifyUI("import_error", "Adding this import would create a circular dependency");
		return;
	}

	// Transitive cycle detection: check if the target (or anything it imports)
	// imports our current file. After ComposeSingleManifest, any PU originating
	// from our file will have sourceManifestPath == our file path.
	if (mData->filePath[0] != '\0')
	{
		bool hasCycle = false;
		for (unsigned int i = 0; i < targetManifest.processingUnits.Size(); ++i)
		{
			if (targetManifest.processingUnits[i].sourceManifestPath.Length() > 0 &&
				strcmp(targetManifest.processingUnits[i].sourceManifestPath.AsCStr(), mData->filePath) == 0)
			{
				hasCycle = true;
				break;
			}
		}
		if (hasCycle)
		{
			NotifyUI("import_error", "Adding this import would create a circular dependency");
			return;
		}
	}

	// Add the typed import
	TypedImport newImport(path, TypedImport::ImportType::kManifest);
	mData->manifest.imports.Add(newImport);

	// Merge the resolved PUs into the existing manifest (preserving local edits)
	for (unsigned int i = 0; i < targetManifest.processingUnits.Size(); ++i)
	{
		const ApplicationManifest::ProcessingUnitEntry& sourcePU = targetManifest.processingUnits[i];

		// Skip if a PU with this instance_id already exists
		bool duplicate = false;
		for (unsigned int j = 0; j < mData->manifest.processingUnits.Size(); ++j)
		{
			if (mData->manifest.processingUnits[j].instanceId == sourcePU.instanceId)
			{
				duplicate = true;
				break;
			}
		}
		if (duplicate) continue;

		// Deep-copy PU into our manifest
		mData->manifest.processingUnits.AddDefault();
		ApplicationManifest::ProcessingUnitEntry& newPU =
			mData->manifest.processingUnits[mData->manifest.processingUnits.Size() - 1];

		newPU.typeId = sourcePU.typeId;
		newPU.instanceId = sourcePU.instanceId;
		newPU.frequencyHz = sourcePU.frequencyHz;
		newPU.dedicatedThread = sourcePU.dedicatedThread;
		newPU.root = sourcePU.root;
		newPU.initialPhase = sourcePU.initialPhase;
		newPU.sourceManifestPath = sourcePU.sourceManifestPath.Length() > 0
			? sourcePU.sourceManifestPath : Dia::Core::Containers::String256(path);

		if (sourcePU.config != nullptr)
			newPU.config = new Json::Value(*sourcePU.config);

		for (unsigned int k = 0; k < sourcePU.phases.Size(); ++k)
		{
			ApplicationManifest::PhaseEntry newPhase;
			newPhase.typeId = sourcePU.phases[k].typeId;
			newPhase.instanceId = sourcePU.phases[k].instanceId;
			newPhase.sourceManifestPath = sourcePU.phases[k].sourceManifestPath;
			if (sourcePU.phases[k].config != nullptr)
				newPhase.config = new Json::Value(*sourcePU.phases[k].config);
			newPU.phases.Add(newPhase);
		}

		for (unsigned int k = 0; k < sourcePU.modules.Size(); ++k)
		{
			ApplicationManifest::ModuleEntry newModule;
			newModule.typeId = sourcePU.modules[k].typeId;
			newModule.instanceId = sourcePU.modules[k].instanceId;
			newModule.sourceManifestPath = sourcePU.modules[k].sourceManifestPath;
			if (sourcePU.modules[k].config != nullptr)
				newModule.config = new Json::Value(*sourcePU.modules[k].config);
			for (unsigned int m = 0; m < sourcePU.modules[k].phaseIds.Size(); ++m)
				newModule.phaseIds.Add(sourcePU.modules[k].phaseIds[m]);
			for (unsigned int m = 0; m < sourcePU.modules[k].dependencies.Size(); ++m)
				newModule.dependencies.Add(sourcePU.modules[k].dependencies[m]);
			newPU.modules.Add(newModule);
		}

		for (unsigned int k = 0; k < sourcePU.transitions.Size(); ++k)
			newPU.transitions.Add(sourcePU.transitions[k]);
	}

	MarkDirty();
	NotifyManifestUpdated();
}

void DiaApplicationEditor::HandleRemoveImport(const Json::Value& data)
{
	if (!mData->hasManifest) return;
	if (!data.isMember("path")) return;

	const char* path = data["path"].asCString();
	bool found = false;

	for (unsigned int i = 0; i < mData->manifest.imports.Size(); ++i)
	{
		if (strcmp(mData->manifest.imports[i].path.AsCStr(), path) == 0)
		{
			mData->manifest.imports.RemoveAt(i);
			found = true;
			break;
		}
	}

	if (!found) return;

	// Remove PUs that came from the removed import
	for (unsigned int i = mData->manifest.processingUnits.Size(); i > 0; --i)
	{
		unsigned int idx = i - 1;
		if (mData->manifest.processingUnits[idx].sourceManifestPath.Length() > 0 &&
			strcmp(mData->manifest.processingUnits[idx].sourceManifestPath.AsCStr(), path) == 0)
		{
			mData->manifest.processingUnits.RemoveAt(idx);
		}
	}

	MarkDirty();
	NotifyManifestUpdated();
}

REGISTER_EDITOR_PLUGIN(DiaApplicationEditor, "DiaApplicationEditor")
