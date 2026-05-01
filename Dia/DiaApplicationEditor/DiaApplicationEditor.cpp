#include "DiaApplicationEditor/DiaApplicationEditor.h"
#include "DiaApplicationEditor/ManifestSerializer.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/UI/WebUIBridge.h>

#include <DiaApplication/Manifest/ApplicationManifestLoader.h>
#include <DiaApplication/Manifest/ManifestValidator.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>

#include <DiaCore/Json/external/json/json.h>

#include <fstream>
#include <cstring>
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

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
		mBridge->UnregisterEventHandler(StringCRC("remove_node"));
		mBridge->UnregisterEventHandler(StringCRC("reorder_node"));
		mBridge->UnregisterEventHandler(StringCRC("phase_positions_changed"));
		mBridge->UnregisterEventHandler(StringCRC("transition_added"));
		mBridge->UnregisterEventHandler(StringCRC("transition_removed"));
		mBridge->UnregisterEventHandler(StringCRC("transition_condition_changed"));
		mBridge->UnregisterEventHandler(StringCRC("module_config_changed"));
		mBridge->UnregisterEventHandler(StringCRC("resolve_conflict"));
		mBridge->UnregisterEventHandler(StringCRC("node_selected"));
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
		OPENFILENAMEA ofn = {};
		ofn.lStructSize  = sizeof(ofn);
		ofn.lpstrFilter  = "Dia App Manifest (*.diaapp)\0*.diaapp\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile    = dialogPath;
		ofn.nMaxFile     = MAX_PATH;
		ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
		ofn.lpstrDefExt  = "diaapp";
		if (!GetOpenFileNameA(&ofn))
			return;
		path = dialogPath;
	}

	// Check for unsaved changes — notify UI to prompt user
	if (IsDirty())
	{
		NotifyUI("prompt_unsaved_changes", path);
		return;
	}

	// Load via ApplicationManifestLoader (empty registry — type validation deferred to Phase 3)
	ApplicationTypeRegistry registry;
	ApplicationManifestLoader loader(registry);

	ApplicationManifest newManifest;
	ManifestValidationResult result = loader.LoadFromFile(path, newManifest);

	// kUnknownType is non-fatal in the editor — type registry is empty offline;
	// live type validation is deferred to Phase 3. Any other error means the file
	// could not be structurally parsed and should be rejected.
	if (result != ManifestValidationResult::kSuccess &&
		result != ManifestValidationResult::kUnknownType)
	{
		NotifyUI("file_error", "Failed to parse manifest");
		return;
	}

	mData->manifest = newManifest;
	strncpy_s(mData->filePath, sizeof(mData->filePath), path, _TRUNCATE);
	mData->isDirty = false;
	mData->hasManifest = true;

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

bool DiaApplicationEditor::WriteManifestToDisk(const char* path)
{
	// Create .bak backup if file already exists
	std::ifstream existingFile(path);
	if (existingFile.good())
	{
		existingFile.close();
		char backupPath[512 + 4];
		snprintf(backupPath, sizeof(backupPath), "%s.bak", path);
		std::rename(path, backupPath);
	}

	// Serialize manifest IR to JSON
	Json::Value json;
	if (!ManifestSerializer::Serialize(mData->manifest, json))
	{
		NotifyUI("file_error", "Failed to serialize manifest");
		return false;
	}

	// Write to disk
	std::ofstream file(path);
	if (!file.is_open())
	{
		NotifyUI("file_error", "Cannot open file for writing");
		return false;
	}

	Json::StyledWriter writer;
	file << writer.write(json);
	file.close();

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

	// Trigger type selector in UI — JS calls back add_node_confirmed with selected type
	Json::Value payload;
	payload["parent_id"] = parentId;
	payload["node_type"] = nodeType;
	if (mBridge != nullptr)
		mBridge->NotifyUIDataChanged("show_type_selector", payload);
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

REGISTER_EDITOR_PLUGIN(DiaApplicationEditor, "DiaApplicationEditor")
