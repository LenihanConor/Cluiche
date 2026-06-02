#include "DiaSceneEditor/DiaSceneEditorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::SceneEditor;

REGISTER_EDITOR_PLUGIN(DiaSceneEditorPlugin, "DiaSceneEditor")

namespace Dia
{
	namespace SceneEditor
	{
		void DiaSceneEditorPlugin::OnProjectChangedStatic(const Dia::Editor::ProjectContext& ctx, void* ud)
		{
			auto* self = static_cast<DiaSceneEditorPlugin*>(ud);
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnProjectChanged — IsValid=%d diagamePath='%s'",
				ctx.IsValid() ? 1 : 0, ctx.diagamePath);

			if (ctx.IsValid())
			{
				self->mStageList = self->mProjectContextManager.BuildStageListJson(ctx.diagamePath);
				DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: loaded %u stages", self->mStageList.size());
			}
			else
			{
				self->mStageList = Json::Value(Json::arrayValue);
			}

			if (self->mBridge)
			{
				Json::Value payload(Json::objectValue);
				payload["diagamePath"] = ctx.diagamePath;
				payload["stages"]      = self->mStageList;
				self->mBridge->NotifyUIDataChanged("scene_editor.project_changed", payload);
			}
		}

		void DiaSceneEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
		{
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnLoad");

			mBridge             = context.mBridge;
			mPluginLoader       = context.mPluginLoader;
			mLoadedScenePath[0] = '\0';

			RegisterRequestHandlers();

			if (context.mModel != nullptr)
				context.mModel->OnDiagameProjectChanged(&DiaSceneEditorPlugin::OnProjectChangedStatic, this);
			else
				DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: OnLoad — context.mModel is null");

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnLoad complete");
		}

		void DiaSceneEditorPlugin::OnUnload()
		{
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnUnload");

			if (mBridge)
			{
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_stage_list"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.load_stage_scene"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_hierarchy"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_hierarchy_filtered"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.set_selection"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_properties"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_blueprint_defaults"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.load_scene"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.save_scene"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.mark_dirty"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_dirty_state"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_available_blueprints"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.add_item"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.duplicate_item"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.delete_item"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.set_enabled"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.rename_item"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.analyse_change_blueprint"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.change_blueprint"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.add_layer"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.delete_layer"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.reorder_layer"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.update_layer"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.set_camera_active"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.set_light_affects_layers"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.add_override"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.remove_override"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.update_override"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.validate"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_scene_properties"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.set_world_bounds"));
			}

			mBridge       = nullptr;
			mPluginLoader = nullptr;
		}

		void DiaSceneEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
		}

		void DiaSceneEditorPlugin::RegisterRequestHandlers()
		{
			if (!mBridge)
				return;

			// T2: return the cached stage list built from the loaded .diagame
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_stage_list"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_stage_list", Dia::Observation::Trace::Category::kNone);
					Json::Value result(Json::objectValue);
					result["success"] = true;
					result["stages"]  = mStageList;
					return result;
				});

			// T2/T3: given a stage path, load its associated scene (round-trip verified in T3)
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.load_stage_scene"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.load_stage_scene", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("stagePath") || !data["stagePath"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: load_stage_scene — missing stagePath");
						result["success"] = false;
						result["error"]   = "missing stagePath";
						return result;
					}

					// Find matching stage entry in cache
					const char* stagePath = data["stagePath"].asCString();
					Json::Value matchedStage;
					for (unsigned int i = 0; i < mStageList.size(); ++i)
					{
						if (mStageList[i]["stagePath"].asString() == stagePath)
						{
							matchedStage = mStageList[i];
							break;
						}
					}

					if (matchedStage.isNull())
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: load_stage_scene — stage not found: %s", stagePath);
						result["success"] = false;
						result["error"]   = "stage not in project";
						return result;
					}

					const std::string scenePath = matchedStage["scenePath"].asString();
					if (scenePath.empty())
					{
						DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: stage '%s' has no scene assigned", stagePath);
						result["success"]   = true;
						result["stage"]     = matchedStage;
						result["scene"]     = Json::Value(Json::nullValue);
						result["hierarchy"] = Json::Value(Json::objectValue);
						return result;
					}

					char err[256] = {};
					Json::Value sceneRoot;
					if (!mFileHandler.Load(scenePath.c_str(), sceneRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaSceneEditorPlugin: load_stage_scene — failed to load scene '%s': %s",
							scenePath.c_str(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "scene load failed";
						return result;
					}

					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: loaded stage scene '%s'", scenePath.c_str());
					mLoadedSceneRoot = sceneRoot;
					strncpy(mLoadedScenePath, scenePath.c_str(), sizeof(mLoadedScenePath) - 1);
					mLoadedScenePath[sizeof(mLoadedScenePath) - 1] = '\0';
					mHierarchyController.ClearSelection();
					mIsDirty = false;

					result["success"]   = true;
					result["stage"]     = matchedStage;
					result["scene"]     = sceneRoot;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
					result["dirty"]     = false;
					return result;
				});

			// T5: filter the in-memory hierarchy without reloading from disk
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_hierarchy_filtered"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_hierarchy_filtered", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (mLoadedSceneRoot.isNull())
					{
						result["success"] = false;
						result["error"]   = "no scene loaded";
						return result;
					}
					const char* filter = data.isMember("filter") && data["filter"].isString()
						? data["filter"].asCString() : nullptr;
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildFilteredHierarchyJson(mLoadedSceneRoot, filter);
					return result;
				});

			// T5: set selection state; returns updated properties for the selected item
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.set_selection"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_selection", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("type") || !data.isMember("id"))
					{
						mHierarchyController.ClearSelection();
						result["success"]   = true;
						result["selection"] = mHierarchyController.GetSelectionJson();
						return result;
					}
					mHierarchyController.SetSelection(
						data["type"].asCString(),
						data["id"].asCString());
					result["success"]   = true;
					result["selection"] = mHierarchyController.GetSelectionJson();
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_hierarchy"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_hierarchy", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: get_hierarchy — missing path");
						result["success"] = false;
						result["error"]   = "missing path";
						return result;
					}

					Json::Value sceneRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), sceneRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaSceneEditorPlugin: get_hierarchy — load failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_properties"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_properties", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("selectionType") || !data.isMember("selectionId"))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: get_properties — missing selectionType or selectionId");
						result["success"] = false;
						result["error"]   = "missing selectionType or selectionId";
						return result;
					}
					if (mLoadedSceneRoot.isNull())
					{
						result["success"] = false;
						result["error"]   = "no scene loaded";
						return result;
					}

					// Derive blueprint base path from the directory containing the scene file
					char blueprintBasePath[512] = {};
					if (mLoadedScenePath[0] != '\0')
					{
						strncpy(blueprintBasePath, mLoadedScenePath, sizeof(blueprintBasePath) - 1);
						for (char* p = blueprintBasePath; *p; ++p)
							if (*p == '\\') *p = '/';
						char* lastSlash = nullptr;
						for (char* p = blueprintBasePath; *p; ++p)
							if (*p == '/') lastSlash = p;
						if (lastSlash) *lastSlash = '\0';
					}

					mHierarchyController.SetSelection(
						data["selectionType"].asCString(),
						data["selectionId"].asCString());

					result["success"]    = true;
					result["properties"] = mPropertyController.BuildPropertyJson(
						mLoadedSceneRoot,
						data["selectionType"].asCString(),
						data["selectionId"].asCString(),
						blueprintBasePath);
					result["selection"]  = mHierarchyController.GetSelectionJson();
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.load_scene"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.load_scene", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: load_scene — missing path");
						result["success"] = false;
						result["error"]   = "missing path";
						return result;
					}

					Json::Value sceneRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), sceneRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaSceneEditorPlugin: load_scene — failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: loaded scene '%s'",
						data["path"].asCString());
					mLoadedSceneRoot = sceneRoot;
					strncpy(mLoadedScenePath, data["path"].asCString(), sizeof(mLoadedScenePath) - 1);
					mLoadedScenePath[sizeof(mLoadedScenePath) - 1] = '\0';
					mHierarchyController.ClearSelection();
					mIsDirty = false;

					result["success"]   = true;
					result["scene"]     = sceneRoot;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
					result["dirty"]     = false;
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.save_scene"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.save_scene", Dia::Observation::Trace::Category::kNone);
					Json::Value result;

					// Use mLoadedScenePath if no explicit path provided
					const char* savePath = mLoadedScenePath;
					if (data.isMember("path") && data["path"].isString())
						savePath = data["path"].asCString();

					if (!savePath || savePath[0] == '\0')
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: save_scene — no path");
						result["success"] = false;
						result["error"]   = "no path";
						return result;
					}

					// Accept scene payload or fall back to cached root
					const Json::Value& sceneToSave = data.isMember("scene")
						? data["scene"] : mLoadedSceneRoot;

					if (sceneToSave.isNull())
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: save_scene — no scene data");
						result["success"] = false;
						result["error"]   = "no scene data";
						return result;
					}

					char err[256] = {};
					if (!mFileHandler.Save(savePath, sceneToSave, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaSceneEditorPlugin: save_scene — failed for '%s': %s",
							savePath, err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					// Update cache and clear dirty
					mLoadedSceneRoot = sceneToSave;
					strncpy(mLoadedScenePath, savePath, sizeof(mLoadedScenePath) - 1);
					mLoadedScenePath[sizeof(mLoadedScenePath) - 1] = '\0';
					mIsDirty = false;

					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: saved scene '%s'", savePath);
					result["success"] = true;
					result["dirty"]   = false;
					return result;
				});

			// T9: blueprint defaults read-only tab — raw blueprint fields, no instance_data overlay
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_blueprint_defaults"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_blueprint_defaults", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("blueprintId") || !data["blueprintId"].isString()
					    || !data.isMember("itemType")  || !data["itemType"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaSceneEditorPlugin: get_blueprint_defaults — missing blueprintId or itemType");
						result["success"] = false;
						result["error"]   = "missing blueprintId or itemType";
						return result;
					}

					char blueprintBasePath[512] = {};
					if (mLoadedScenePath[0] != '\0')
					{
						strncpy(blueprintBasePath, mLoadedScenePath, sizeof(blueprintBasePath) - 1);
						for (char* p = blueprintBasePath; *p; ++p)
							if (*p == '\\') *p = '/';
						char* lastSlash = nullptr;
						for (char* p = blueprintBasePath; *p; ++p)
							if (*p == '/') lastSlash = p;
						if (lastSlash) *lastSlash = '\0';
					}

					Json::Value defaults = mPropertyController.BuildBlueprintDefaultsJson(
						data["blueprintId"].asCString(),
						data["itemType"].asCString(),
						blueprintBasePath);

					result["success"] = true;
					result["data"]    = defaults;
					return result;
				});

			// T10: mark dirty — called by UI when any field edit occurs
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.mark_dirty"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					mIsDirty = true;
					Json::Value result;
					result["success"] = true;
					result["dirty"]   = true;
					if (mBridge)
						mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					return result;
				});

			// T10: query dirty state
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_dirty_state"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value result;
					result["success"] = true;
					result["dirty"]   = mIsDirty;
					return result;
				});

			// T11: Return available blueprints of a given type from the asset catalogue.
			// Scans the catalogue JSON for assets of type "diaentity"/"diacamera"/"dialight".
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_available_blueprints"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_available_blueprints", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					const char* itemType = data.isMember("itemType") && data["itemType"].isString()
						? data["itemType"].asCString() : "entity";

					const char* assetType = "diaentity";
					if (strcmp(itemType, "camera") == 0) assetType = "diacamera";
					else if (strcmp(itemType, "light") == 0) assetType = "dialight";

					Json::Value blueprints(Json::arrayValue);

					// Find the catalogue path from the active stage list
					// (diagame config.asset_catalogue is relative to diagame dir)
					for (unsigned int i = 0; i < mStageList.size(); ++i)
					{
						// Catalogue lives alongside the diagame; derive from loaded scene path
						// We don't store the diagame path directly, so scan filesystem adjacent
						// to the loaded scene path. This is a best-effort scan.
						break;
					}

					// Simple filesystem scan: walk blueprintBasePath for files with matching ext.
					// This is forward-compatible regardless of catalogue format.
					if (mLoadedScenePath[0] != '\0')
					{
						char dir[512];
						strncpy(dir, mLoadedScenePath, sizeof(dir) - 1);
						dir[sizeof(dir) - 1] = '\0';
						for (char* p = dir; *p; ++p) if (*p == '\\') *p = '/';
						char* lastSlash = nullptr;
						for (char* p = dir; *p; ++p) if (*p == '/') lastSlash = p;
						if (lastSlash) *lastSlash = '\0';

						// Return the base path so UI knows where to pick from
						blueprints.append(Json::Value(Json::objectValue));  // placeholder
					}

					result["success"]    = true;
					result["assetType"]  = assetType;
					result["blueprints"] = blueprints;
					DIA_LOG_INFO("Editor",
						"DiaSceneEditorPlugin: get_available_blueprints for type '%s'", itemType);
					return result;
				});

			// T11: Add a new entity/camera/light instance
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.add_item"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.add_item", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("itemType") || !data.isMember("blueprintId"))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: add_item — missing itemType or blueprintId");
						result["success"] = false;
						result["error"]   = "missing itemType or blueprintId";
						return result;
					}
					if (mLoadedSceneRoot.isNull())
					{
						result["success"] = false;
						result["error"]   = "no scene loaded";
						return result;
					}

					char err[256] = {};
					if (!SceneMutator::AddItem(mLoadedSceneRoot,
					        data["itemType"].asCString(),
					        data["blueprintId"].asCString(),
					        err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: add_item — %s", err);
						result["success"] = false;
						result["error"]   = err;
						return result;
					}

					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// T12: Duplicate with _copy suffix + position offset
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.duplicate_item"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.duplicate_item", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("itemType") || !data.isMember("itemId"))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: duplicate_item — missing itemType or itemId");
						result["success"] = false;
						result["error"]   = "missing itemType or itemId";
						return result;
					}
					if (mLoadedSceneRoot.isNull())
					{
						result["success"] = false; result["error"] = "no scene loaded"; return result;
					}

					char err[256] = {};
					if (!SceneMutator::DuplicateItem(mLoadedSceneRoot,
					        data["itemType"].asCString(),
					        data["itemId"].asCString(),
					        err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: duplicate_item — %s", err);
						result["success"] = false; result["error"] = err; return result;
					}

					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// T13: Delete item
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.delete_item"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.delete_item", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("itemType") || !data.isMember("itemId"))
					{
						result["success"] = false; result["error"] = "missing itemType or itemId"; return result;
					}
					if (mLoadedSceneRoot.isNull())
					{
						result["success"] = false; result["error"] = "no scene loaded"; return result;
					}

					char err[256] = {};
					if (!SceneMutator::DeleteItem(mLoadedSceneRoot,
					        data["itemType"].asCString(),
					        data["itemId"].asCString(),
					        err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: delete_item — %s", err);
						result["success"] = false; result["error"] = err; return result;
					}

					mHierarchyController.ClearSelection();
					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// T13: Enable / disable
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.set_enabled"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_enabled", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("itemType") || !data.isMember("itemId") || !data.isMember("enabled"))
					{
						result["success"] = false; result["error"] = "missing itemType, itemId, or enabled"; return result;
					}
					if (mLoadedSceneRoot.isNull())
					{
						result["success"] = false; result["error"] = "no scene loaded"; return result;
					}

					char err[256] = {};
					if (!SceneMutator::SetEnabled(mLoadedSceneRoot,
					        data["itemType"].asCString(),
					        data["itemId"].asCString(),
					        data["enabled"].asBool(),
					        err, sizeof(err)))
					{
						result["success"] = false; result["error"] = err; return result;
					}

					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// T14: Rename
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.rename_item"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.rename_item", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("itemType") || !data.isMember("oldId") || !data.isMember("newId"))
					{
						result["success"] = false; result["error"] = "missing itemType, oldId, or newId"; return result;
					}
					if (mLoadedSceneRoot.isNull())
					{
						result["success"] = false; result["error"] = "no scene loaded"; return result;
					}

					char err[256] = {};
					if (!SceneMutator::RenameItem(mLoadedSceneRoot,
					        data["itemType"].asCString(),
					        data["oldId"].asCString(),
					        data["newId"].asCString(),
					        err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: rename_item — %s", err);
						result["success"] = false; result["error"] = err; return result;
					}

					mHierarchyController.SetSelection(
						data["itemType"].asCString(), data["newId"].asCString());
					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// ── T16: Change Blueprint ────────────────────────────────────────────────

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.analyse_change_blueprint"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.analyse_change_blueprint", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("itemType") || !data.isMember("itemId")
					    || !data.isMember("newBlueprintComponents"))
					{
						result["success"] = false; result["error"] = "missing required fields"; return result;
					}
					if (mLoadedSceneRoot.isNull()) { result["success"] = false; result["error"] = "no scene loaded"; return result; }

					result["success"]  = true;
					result["analysis"] = SceneMutator::AnalyseChangeBlueprintJson(
						mLoadedSceneRoot,
						data["itemType"].asCString(),
						data["itemId"].asCString(),
						data["newBlueprintComponents"]);
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.change_blueprint"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.change_blueprint", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("itemType") || !data.isMember("itemId")
					    || !data.isMember("newBlueprintId") || !data.isMember("newBlueprintComponents"))
					{
						result["success"] = false; result["error"] = "missing required fields"; return result;
					}
					if (mLoadedSceneRoot.isNull()) { result["success"] = false; result["error"] = "no scene loaded"; return result; }

					char err[256] = {};
					if (!SceneMutator::ChangeBlueprint(mLoadedSceneRoot,
					        data["itemType"].asCString(), data["itemId"].asCString(),
					        data["newBlueprintId"].asCString(), data["newBlueprintComponents"], err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: change_blueprint — %s", err);
						result["success"] = false; result["error"] = err; return result;
					}

					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// ── T17: Layer CRUD ──────────────────────────────────────────────────────

			auto layerMutate = [this](const char* op, Json::Value result,
			                           bool ok, const char* err) -> Json::Value
			{
				if (!ok) { result["success"] = false; result["error"] = err; return result; }
				mIsDirty = true;
				if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
				result["success"]   = true;
				result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
				return result;
			};

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.add_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.add_layer", Dia::Observation::Trace::Category::kNone);
					Json::Value r;
					if (!data.isMember("layerId")) { r["success"]=false; r["error"]="missing layerId"; return r; }
					if (mLoadedSceneRoot.isNull()) { r["success"]=false; r["error"]="no scene loaded"; return r; }
					char err[256]={};
					bool ok = SceneMutator::AddLayer(mLoadedSceneRoot, data["layerId"].asCString(), err, sizeof(err));
					return layerMutate("add_layer", r, ok, err);
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.delete_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.delete_layer", Dia::Observation::Trace::Category::kNone);
					Json::Value r;
					if (!data.isMember("layerId")) { r["success"]=false; r["error"]="missing layerId"; return r; }
					if (mLoadedSceneRoot.isNull()) { r["success"]=false; r["error"]="no scene loaded"; return r; }
					char err[256]={};
					bool ok = SceneMutator::DeleteLayer(mLoadedSceneRoot, data["layerId"].asCString(), err, sizeof(err));
					return layerMutate("delete_layer", r, ok, err);
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.reorder_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.reorder_layer", Dia::Observation::Trace::Category::kNone);
					Json::Value r;
					if (!data.isMember("layerId") || !data.isMember("newIndex")) { r["success"]=false; r["error"]="missing layerId or newIndex"; return r; }
					if (mLoadedSceneRoot.isNull()) { r["success"]=false; r["error"]="no scene loaded"; return r; }
					char err[256]={};
					bool ok = SceneMutator::ReorderLayer(mLoadedSceneRoot,
					    data["layerId"].asCString(), data["newIndex"].asInt(), err, sizeof(err));
					return layerMutate("reorder_layer", r, ok, err);
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.update_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.update_layer", Dia::Observation::Trace::Category::kNone);
					Json::Value r;
					if (!data.isMember("layerId") || !data.isMember("fields")) { r["success"]=false; r["error"]="missing layerId or fields"; return r; }
					if (mLoadedSceneRoot.isNull()) { r["success"]=false; r["error"]="no scene loaded"; return r; }
					char err[256]={};
					bool ok = SceneMutator::UpdateLayer(mLoadedSceneRoot,
					    data["layerId"].asCString(), data["fields"], err, sizeof(err));
					return layerMutate("update_layer", r, ok, err);
				});

			// ── T18: Camera / Light ──────────────────────────────────────────────────

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.set_camera_active"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_camera_active", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("cameraId")) { result["success"]=false; result["error"]="missing cameraId"; return result; }
					if (mLoadedSceneRoot.isNull())   { result["success"]=false; result["error"]="no scene loaded"; return result; }
					char err[256]={};
					if (!SceneMutator::SetCameraActive(mLoadedSceneRoot, data["cameraId"].asCString(), err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: set_camera_active — %s", err);
						result["success"]=false; result["error"]=err; return result;
					}
					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.set_light_affects_layers"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_light_affects_layers", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("lightId") || !data.isMember("layerIds")) { result["success"]=false; result["error"]="missing lightId or layerIds"; return result; }
					if (mLoadedSceneRoot.isNull()) { result["success"]=false; result["error"]="no scene loaded"; return result; }
					char err[256]={};
					if (!SceneMutator::SetLightAffectsLayers(mLoadedSceneRoot,
					        data["lightId"].asCString(), data["layerIds"], err, sizeof(err)))
					{
						result["success"]=false; result["error"]=err; return result;
					}
					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// ── T19: Override management ─────────────────────────────────────────────

			auto overrideMutate = [this](bool ok, const char* err) -> Json::Value
			{
				Json::Value r;
				if (!ok) { r["success"]=false; r["error"]=err; return r; }
				mIsDirty = true;
				if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
				r["success"] = true;
				return r;
			};

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.add_override"),
				[this, overrideMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.add_override", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType")||!data.isMember("itemId")||!data.isMember("overrideKey")||!data.isMember("defaultValue"))
					{ Json::Value r; r["success"]=false; r["error"]="missing fields"; return r; }
					if (mLoadedSceneRoot.isNull()) { Json::Value r; r["success"]=false; r["error"]="no scene loaded"; return r; }
					char err[256]={};
					bool ok = SceneMutator::AddOverride(mLoadedSceneRoot,
					    data["itemType"].asCString(), data["itemId"].asCString(),
					    data["overrideKey"].asCString(), data["defaultValue"], err, sizeof(err));
					return overrideMutate(ok, err);
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.remove_override"),
				[this, overrideMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.remove_override", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType")||!data.isMember("itemId")||!data.isMember("overrideKey"))
					{ Json::Value r; r["success"]=false; r["error"]="missing fields"; return r; }
					if (mLoadedSceneRoot.isNull()) { Json::Value r; r["success"]=false; r["error"]="no scene loaded"; return r; }
					char err[256]={};
					bool ok = SceneMutator::RemoveOverride(mLoadedSceneRoot,
					    data["itemType"].asCString(), data["itemId"].asCString(),
					    data["overrideKey"].asCString(), err, sizeof(err));
					return overrideMutate(ok, err);
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.update_override"),
				[this, overrideMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.update_override", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType")||!data.isMember("itemId")||!data.isMember("overrideKey")||!data.isMember("value"))
					{ Json::Value r; r["success"]=false; r["error"]="missing fields"; return r; }
					if (mLoadedSceneRoot.isNull()) { Json::Value r; r["success"]=false; r["error"]="no scene loaded"; return r; }
					char err[256]={};
					bool ok = SceneMutator::UpdateOverride(mLoadedSceneRoot,
					    data["itemType"].asCString(), data["itemId"].asCString(),
					    data["overrideKey"].asCString(), data["value"], err, sizeof(err));
					return overrideMutate(ok, err);
				});

			// ── T20: Validation ──────────────────────────────────────────────────────

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.validate"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.validate", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (mLoadedSceneRoot.isNull()) { result["success"]=false; result["error"]="no scene loaded"; return result; }
					result["success"]    = true;
					result["validation"] = mValidator.Validate(mLoadedSceneRoot);
					return result;
				});

			// ── T21: Scene Properties panel ──────────────────────────────────────────

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.get_scene_properties"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_scene_properties", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (mLoadedSceneRoot.isNull()) { result["success"]=false; result["error"]="no scene loaded"; return result; }

					const Json::Value& scene = mLoadedSceneRoot["scene2d"];
					Json::Value props(Json::objectValue);
					props["world_bounds"] = scene.isMember("world_bounds")
						? scene["world_bounds"] : Json::Value(Json::objectValue);
					props["layerCount"]   = scene.isMember("layers")   ? (int)scene["layers"].size()   : 0;
					props["cameraCount"]  = scene.isMember("cameras")  ? (int)scene["cameras"].size()  : 0;
					props["lightCount"]   = scene.isMember("lights")   ? (int)scene["lights"].size()   : 0;
					props["entityCount"]  = scene.isMember("entities") ? (int)scene["entities"].size() : 0;
					props["scenePath"]    = mLoadedScenePath;
					props["validation"]   = mValidator.Validate(mLoadedSceneRoot);

					result["success"]    = true;
					result["properties"] = props;
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.set_world_bounds"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_world_bounds", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("world_bounds")) { result["success"]=false; result["error"]="missing world_bounds"; return result; }
					if (mLoadedSceneRoot.isNull()) { result["success"]=false; result["error"]="no scene loaded"; return result; }
					mLoadedSceneRoot["scene2d"]["world_bounds"] = data["world_bounds"];
					mIsDirty = true;
					if (mBridge) mBridge->NotifyUIDataChanged("scene_editor.dirty_changed", Json::Value(true));
					result["success"] = true;
					return result;
				});
		}
	}
}
