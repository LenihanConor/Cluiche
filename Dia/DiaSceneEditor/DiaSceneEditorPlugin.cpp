#include "DiaSceneEditor/DiaSceneEditorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <sstream>

using namespace Dia::SceneEditor;

REGISTER_EDITOR_PLUGIN(DiaSceneEditorPlugin, "DiaSceneEditor")

namespace Dia
{
	namespace SceneEditor
	{
		void DiaSceneEditorPlugin::OnProjectChanged(const Dia::Editor::ProjectContext& ctx)
		{
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnProjectChanged — IsValid=%d diagamePath='%s'",
				ctx.IsValid() ? 1 : 0, ctx.diagamePath);

			strncpy_s(mDiagamePath, sizeof(mDiagamePath),
			          ctx.IsValid() ? ctx.diagamePath : "", _TRUNCATE);

			if (ctx.IsValid())
			{
				mStageList = mProjectContextManager.BuildStageListJson(ctx.diagamePath);
				DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: loaded %u stages", mStageList.size());
			}
			else
			{
				mStageList = Json::Value(Json::arrayValue);
			}

			if (GetBridge())
			{
				Json::Value payload(Json::objectValue);
				payload["diagamePath"] = ctx.diagamePath;
				payload["isValid"]     = ctx.IsValid();
				payload["stages"]      = mStageList;
				GetBridge()->NotifyUIDataChanged("scene_editor.project_changed", payload);
			}
		}

		void DiaSceneEditorPlugin::OnPluginLoad()
		{
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnPluginLoad");

			mLoadedScenePath[0]    = '\0';
			mSceneCatalogueId[0]   = '\0';

			if (GetModel() != nullptr)
			{
				const Dia::Editor::ProjectContext& proj = GetModel()->GetDiagameProject();
				strncpy_s(mDiagamePath, sizeof(mDiagamePath),
				          proj.IsValid() ? proj.diagamePath : "", _TRUNCATE);
				if (proj.IsValid())
					mStageList = mProjectContextManager.BuildStageListJson(proj.diagamePath);
			}
			else
				DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: OnPluginLoad — model is null");

			RegisterRequestHandlers();

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnPluginLoad complete");
		}

		void DiaSceneEditorPlugin::OnPluginUnload()
		{
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnPluginUnload");
			mLoadedSceneRoot = Json::Value();
			mLoadedScenePath[0] = '\0';
		}

		void DiaSceneEditorPlugin::OnNavigate(const Dia::Core::StringCRC& instanceId)
		{
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin::OnNavigate: instanceId='%s'", instanceId.AsChar());

			if (!GetBridge())
				return;

			Json::Value req;
			req["id"] = instanceId.AsChar();
			Json::Value rec = GetBridge()->InvokeRequestHandler(
				Dia::Core::StringCRC("asset_catalogue.get_record"), req);

			if (rec.isNull() || !rec.get("success", false).asBool())
			{
				DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin::OnNavigate: get_record failed for '%s'", instanceId.AsChar());
				return;
			}

			const std::string sourcePath = rec["record"].get("source_path", "").asString();
			if (sourcePath.empty())
			{
				DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin::OnNavigate: no source_path for '%s'", instanceId.AsChar());
				return;
			}

			Json::Value loadReq;
			loadReq["path"] = sourcePath;
			Json::Value loadResult = GetBridge()->InvokeRequestHandler(
				Dia::Core::StringCRC("scene_editor.load_scene"), loadReq);

			if (loadResult.isNull() || !loadResult.get("success", false).asBool())
			{
				DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin::OnNavigate: load_scene failed for '%s'", sourcePath.c_str());
				return;
			}

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin::OnNavigate: loaded scene '%s'", sourcePath.c_str());
		}

		void DiaSceneEditorPlugin::ResolveCatalogueIdForLoadedScene()
		{
			mSceneCatalogueId[0] = '\0';

			if (mLoadedScenePath[0] == '\0' || GetBridge() == nullptr)
				return;

			Json::Value stateResult = GetBridge()->InvokeRequestHandler(
				Dia::Core::StringCRC("asset_catalogue.get_state"),
				Json::Value(Json::objectValue));

			if (stateResult.isNull() || !stateResult.isMember("records"))
			{
				DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: scene '%s' not in catalogue — skipping relationship calls", mLoadedScenePath);
				return;
			}

			char normLoaded[512];
			strncpy_s(normLoaded, sizeof(normLoaded), mLoadedScenePath, _TRUNCATE);
			for (char* p = normLoaded; *p; ++p)
				if (*p == '\\') *p = '/';

			const Json::Value& records = stateResult["records"];
			for (unsigned int i = 0; i < records.size(); ++i)
			{
				const Json::Value& rec = records[i];
				if (rec.get("type", "").asString() != "diascene")
					continue;

				std::string srcPath = rec.get("source_path", "").asString();
				for (char& c : srcPath)
					if (c == '\\') c = '/';

				if (srcPath.empty())
					continue;

				const size_t loadedLen = strlen(normLoaded);
				const size_t srcLen    = srcPath.size();
				if (loadedLen >= srcLen &&
				    _stricmp(normLoaded + loadedLen - srcLen, srcPath.c_str()) == 0)
				{
					const std::string id = rec.get("id", "").asString();
					strncpy_s(mSceneCatalogueId, sizeof(mSceneCatalogueId), id.c_str(), _TRUNCATE);
					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: resolved scene catalogue ID '%s' for path '%s'",
						mSceneCatalogueId, mLoadedScenePath);
					return;
				}
			}

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: scene '%s' not in catalogue — skipping relationship calls", mLoadedScenePath);
		}

		void DiaSceneEditorPlugin::RegisterRequestHandlers()
		{
			if (!GetBridge())
				return;

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_project_state"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value r;
					r["isValid"]     = mDiagamePath[0] != '\0';
					r["diagamePath"] = mDiagamePath;
					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: get_project_state polled — isValid=%d diagamePath='%s'",
						mDiagamePath[0] != '\0', mDiagamePath);
					return r;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_stage_list"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_stage_list", Dia::Observation::Trace::Category::kNone);
					Json::Value result(Json::objectValue);
					result["success"] = true;
					result["stages"]  = mStageList;
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.load_stage_scene"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.load_stage_scene", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("stagePath") || !data["stagePath"].isString())
						return MakeErrorResponse("missing stagePath");

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
						return MakeErrorResponse("stage not in project");

					const std::string scenePath = matchedStage["scenePath"].asString();
					if (scenePath.empty())
					{
						Json::Value result;
						result["success"]   = true;
						result["stage"]     = matchedStage;
						result["scene"]     = Json::Value(Json::nullValue);
						result["hierarchy"] = Json::Value(Json::objectValue);
						return result;
					}

					char err[256] = {};
					Json::Value sceneRoot;
					if (!mFileHandler.Load(scenePath.c_str(), sceneRoot, err, sizeof(err)))
						return MakeErrorResponse(err[0] ? err : "scene load failed");

					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: loaded stage scene '%s'", scenePath.c_str());
					mLoadedSceneRoot = sceneRoot;
					strncpy_s(mLoadedScenePath, sizeof(mLoadedScenePath), scenePath.c_str(), _TRUNCATE);
					mHierarchyController.ClearSelection();
					ClearDirty();
					ResolveCatalogueIdForLoadedScene();

					Json::Value result;
					result["success"]   = true;
					result["stage"]     = matchedStage;
					result["scene"]     = sceneRoot;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
					result["dirty"]     = false;
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_hierarchy_filtered"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_hierarchy_filtered", Dia::Observation::Trace::Category::kNone);
					if (mLoadedSceneRoot.isNull())
						return MakeErrorResponse("no scene loaded");
					const char* filter = data.isMember("filter") && data["filter"].isString()
						? data["filter"].asCString() : nullptr;
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildFilteredHierarchyJson(mLoadedSceneRoot, filter);
					return result;
				});

			RegisterHandler(
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

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_hierarchy"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_hierarchy", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString())
						return MakeErrorResponse("missing path");

					Json::Value sceneRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), sceneRoot, err, sizeof(err)))
						return MakeErrorResponse(err[0] ? err : "load failed");

					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_properties"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_properties", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("selectionType") || !data.isMember("selectionId"))
						return MakeErrorResponse("missing selectionType or selectionId");
					if (mLoadedSceneRoot.isNull())
						return MakeErrorResponse("no scene loaded");

					char blueprintBasePath[512] = {};
					if (mLoadedScenePath[0] != '\0')
					{
						strncpy_s(blueprintBasePath, sizeof(blueprintBasePath), mLoadedScenePath, _TRUNCATE);
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

					Json::Value result;
					result["success"]    = true;
					result["properties"] = mPropertyController.BuildPropertyJson(
						mLoadedSceneRoot,
						data["selectionType"].asCString(),
						data["selectionId"].asCString(),
						blueprintBasePath);
					result["selection"]  = mHierarchyController.GetSelectionJson();
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.load_scene"),
				[this](const Json::Value& data) -> Json::Value { return HandleLoadScene(data); });

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.save_scene"),
				[this](const Json::Value& data) -> Json::Value { return HandleSaveScene(data); });

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_blueprint_defaults"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_blueprint_defaults", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("blueprintId") || !data["blueprintId"].isString()
					    || !data.isMember("itemType")  || !data["itemType"].isString())
						return MakeErrorResponse("missing blueprintId or itemType");

					char blueprintBasePath[512] = {};
					if (mLoadedScenePath[0] != '\0')
					{
						strncpy_s(blueprintBasePath, sizeof(blueprintBasePath), mLoadedScenePath, _TRUNCATE);
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

					Json::Value result;
					result["success"] = true;
					result["data"]    = defaults;
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.mark_dirty"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					MarkDirty();
					Json::Value result;
					result["success"] = true;
					result["dirty"]   = true;
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_dirty_state"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value result;
					result["success"] = true;
					result["dirty"]   = IsDirty();
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_available_blueprints"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_available_blueprints", Dia::Observation::Trace::Category::kNone);
					const char* itemType = data.isMember("itemType") && data["itemType"].isString()
						? data["itemType"].asCString() : "entity";

					const char* assetType = "diaentitytemplate";
					if (strcmp(itemType, "camera") == 0) assetType = "diacamera";
					else if (strcmp(itemType, "light") == 0) assetType = "dialight";

					Json::Value blueprints(Json::arrayValue);
					if (mLoadedScenePath[0] != '\0')
					{
						blueprints.append(Json::Value(Json::objectValue));
					}

					Json::Value result;
					result["success"]    = true;
					result["assetType"]  = assetType;
					result["blueprints"] = blueprints;
					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: get_available_blueprints for type '%s'", itemType);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.add_item"),
				[this](const Json::Value& data) -> Json::Value { return HandleAddItem(data); });

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.duplicate_item"),
				[this](const Json::Value& data) -> Json::Value { return HandleDuplicateItem(data); });

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.delete_item"),
				[this](const Json::Value& data) -> Json::Value { return HandleDeleteItem(data); });

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.set_enabled"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_enabled", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType") || !data.isMember("itemId") || !data.isMember("enabled"))
						return MakeErrorResponse("missing itemType, itemId, or enabled");
					if (mLoadedSceneRoot.isNull())
						return MakeErrorResponse("no scene loaded");

					char err[256] = {};
					if (!SceneMutator::SetEnabled(mLoadedSceneRoot,
					        data["itemType"].asCString(),
					        data["itemId"].asCString(),
					        data["enabled"].asBool(),
					        err, sizeof(err)))
						return MakeErrorResponse(err);

					MarkDirty();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.rename_item"),
				[this](const Json::Value& data) -> Json::Value { return HandleRenameItem(data); });

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.analyse_change_blueprint"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.analyse_change_blueprint", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType") || !data.isMember("itemId") || !data.isMember("newBlueprintComponents"))
						return MakeErrorResponse("missing required fields");
					if (mLoadedSceneRoot.isNull())
						return MakeErrorResponse("no scene loaded");

					Json::Value result;
					result["success"]  = true;
					result["analysis"] = SceneMutator::AnalyseChangeBlueprintJson(
						mLoadedSceneRoot,
						data["itemType"].asCString(),
						data["itemId"].asCString(),
						data["newBlueprintComponents"]);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.change_blueprint"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.change_blueprint", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType") || !data.isMember("itemId")
					    || !data.isMember("newBlueprintId") || !data.isMember("newBlueprintComponents"))
						return MakeErrorResponse("missing required fields");
					if (mLoadedSceneRoot.isNull())
						return MakeErrorResponse("no scene loaded");

					char oldBlueprintId[256] = {};
					if (mSceneCatalogueId[0] != '\0')
					{
						const char* arrayKey = nullptr;
						const char* itemTypeStr = data["itemType"].asCString();
						if      (strcmp(itemTypeStr, "entity") == 0) arrayKey = "entities";
						else if (strcmp(itemTypeStr, "camera") == 0) arrayKey = "cameras";
						else if (strcmp(itemTypeStr, "light")  == 0) arrayKey = "lights";

						if (arrayKey && mLoadedSceneRoot.isMember("scene2d"))
						{
							const Json::Value& arr = mLoadedSceneRoot["scene2d"][arrayKey];
							const char* itemIdStr = data["itemId"].asCString();
							char idBuf[256];
							for (unsigned int i = 0; i < arr.size(); ++i)
							{
								if (!arr[i].isMember("id")) continue;
								const Json::Value& idVal = arr[i]["id"];
								const char* v = idVal.isString() ? idVal.asCString()
								              : (idVal.isObject() && idVal.isMember("value") ? idVal["value"].asCString() : "");
								strncpy_s(idBuf, sizeof(idBuf), v, _TRUNCATE);
								if (strcmp(idBuf, itemIdStr) == 0 && arr[i].isMember("blueprint"))
								{
									const Json::Value& bp = arr[i]["blueprint"];
									const char* bpVal = bp.isString() ? bp.asCString()
									                  : (bp.isObject() && bp.isMember("value") ? bp["value"].asCString() : "");
									strncpy_s(oldBlueprintId, sizeof(oldBlueprintId), bpVal, _TRUNCATE);
									break;
								}
							}
						}
					}

					char err[256] = {};
					if (!SceneMutator::ChangeBlueprint(mLoadedSceneRoot,
					        data["itemType"].asCString(), data["itemId"].asCString(),
					        data["newBlueprintId"].asCString(), data["newBlueprintComponents"], err, sizeof(err)))
						return MakeErrorResponse(err);

					if (GetBridge() && mSceneCatalogueId[0] != '\0')
					{
						if (oldBlueprintId[0] != '\0')
						{
							Json::Value removeReq;
							removeReq["from"] = mSceneCatalogueId;
							removeReq["rel"]  = "uses";
							removeReq["to"]   = oldBlueprintId;
							GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.remove_relationship"), removeReq);
						}
						Json::Value addReq;
						addReq["from"] = mSceneCatalogueId;
						addReq["rel"]  = "uses";
						addReq["to"]   = data["newBlueprintId"].asString();
						GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.add_relationship"), addReq);
					}

					MarkDirty();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// ── Layer CRUD ──────────────────────────────────────────────────────

			auto layerMutate = [this](bool ok, const char* err) -> Json::Value
			{
				if (!ok) return MakeErrorResponse(err);
				MarkDirty();
				Json::Value result;
				result["success"]   = true;
				result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
				return result;
			};

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.add_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.add_layer", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("layerId")) return MakeErrorResponse("missing layerId");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					bool ok = SceneMutator::AddLayer(mLoadedSceneRoot, data["layerId"].asCString(), err, sizeof(err));
					return layerMutate(ok, err);
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.delete_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.delete_layer", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("layerId")) return MakeErrorResponse("missing layerId");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					bool ok = SceneMutator::DeleteLayer(mLoadedSceneRoot, data["layerId"].asCString(), err, sizeof(err));
					return layerMutate(ok, err);
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.reorder_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.reorder_layer", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("layerId") || !data.isMember("newIndex")) return MakeErrorResponse("missing layerId or newIndex");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					bool ok = SceneMutator::ReorderLayer(mLoadedSceneRoot,
					    data["layerId"].asCString(), data["newIndex"].asInt(), err, sizeof(err));
					return layerMutate(ok, err);
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.update_layer"),
				[this, layerMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.update_layer", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("layerId") || !data.isMember("fields")) return MakeErrorResponse("missing layerId or fields");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					bool ok = SceneMutator::UpdateLayer(mLoadedSceneRoot,
					    data["layerId"].asCString(), data["fields"], err, sizeof(err));
					return layerMutate(ok, err);
				});

			// ── Camera / Light ──────────────────────────────────────────────────

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.set_camera_active"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_camera_active", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("cameraId")) return MakeErrorResponse("missing cameraId");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					if (!SceneMutator::SetCameraActive(mLoadedSceneRoot, data["cameraId"].asCString(), err, sizeof(err)))
						return MakeErrorResponse(err);
					MarkDirty();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.set_light_affects_layers"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_light_affects_layers", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("lightId") || !data.isMember("layerIds")) return MakeErrorResponse("missing lightId or layerIds");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					if (!SceneMutator::SetLightAffectsLayers(mLoadedSceneRoot,
					        data["lightId"].asCString(), data["layerIds"], err, sizeof(err)))
						return MakeErrorResponse(err);
					MarkDirty();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
					return result;
				});

			// ── Override management ──────────────────────────────────────────────

			auto overrideMutate = [this](bool ok, const char* err) -> Json::Value
			{
				if (!ok) return MakeErrorResponse(err);
				MarkDirty();
				return MakeSuccessResponse();
			};

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.add_override"),
				[this, overrideMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.add_override", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType")||!data.isMember("itemId")||!data.isMember("overrideKey")||!data.isMember("defaultValue"))
						return MakeErrorResponse("missing fields");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					bool ok = SceneMutator::AddOverride(mLoadedSceneRoot,
					    data["itemType"].asCString(), data["itemId"].asCString(),
					    data["overrideKey"].asCString(), data["defaultValue"], err, sizeof(err));
					return overrideMutate(ok, err);
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.remove_override"),
				[this, overrideMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.remove_override", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType")||!data.isMember("itemId")||!data.isMember("overrideKey"))
						return MakeErrorResponse("missing fields");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					bool ok = SceneMutator::RemoveOverride(mLoadedSceneRoot,
					    data["itemType"].asCString(), data["itemId"].asCString(),
					    data["overrideKey"].asCString(), err, sizeof(err));
					return overrideMutate(ok, err);
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.update_override"),
				[this, overrideMutate](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.update_override", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType")||!data.isMember("itemId")||!data.isMember("overrideKey")||!data.isMember("value"))
						return MakeErrorResponse("missing fields");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					bool ok = SceneMutator::UpdateOverride(mLoadedSceneRoot,
					    data["itemType"].asCString(), data["itemId"].asCString(),
					    data["overrideKey"].asCString(), data["value"], err, sizeof(err));
					return overrideMutate(ok, err);
				});

			// ── Validation ───────────────────────────────────────────────────────

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.validate"),
				[this](const Json::Value& data) -> Json::Value { return HandleValidate(data); });

			// ── Scene Properties ─────────────────────────────────────────────────

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.get_scene_properties"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_scene_properties", Dia::Observation::Trace::Category::kNone);
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");

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

					Json::Value result;
					result["success"]    = true;
					result["properties"] = props;
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.set_world_bounds"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_world_bounds", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("world_bounds")) return MakeErrorResponse("missing world_bounds");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					mLoadedSceneRoot["scene2d"]["world_bounds"] = data["world_bounds"];
					MarkDirty();
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.new_scene_shortcut"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.new_scene_shortcut", Dia::Observation::Trace::Category::kNone);
					if (GetPluginLoader())
						GetPluginLoader()->LoadPlugin(
							Dia::Core::StringCRC("DiaAssetCatalogueEditor"),
							Dia::Core::StringCRC("DiaAssetCatalogueEditor"));

					if (GetBridge())
					{
						Json::Value fwd = GetBridge()->InvokeRequestHandler(
							Dia::Core::StringCRC("asset_catalogue.create_scene"), data);
						if (!fwd.isNull())
							return fwd;
					}

					return MakeErrorResponse("Asset Catalogue not available — open the Asset Catalogue panel first");
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.create_asset"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.create_asset", Dia::Observation::Trace::Category::kNone);
					if (GetPluginLoader())
						GetPluginLoader()->LoadPlugin(
							Dia::Core::StringCRC("DiaAssetCatalogueEditor"),
							Dia::Core::StringCRC("DiaAssetCatalogueEditor"));

					if (GetBridge())
					{
						Json::Value fwd = GetBridge()->InvokeRequestHandler(
							Dia::Core::StringCRC("asset_catalogue.create_asset"), data);
						if (!fwd.isNull())
							return fwd;
					}

					return MakeErrorResponse("Asset Catalogue not available — open the Asset Catalogue panel first");
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.associate_scene_to_stage"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.associate_scene_to_stage", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("stagePath") || !data["stagePath"].isString()
						|| !data.isMember("scenePath") || !data["scenePath"].isString())
						return MakeErrorResponse("missing stagePath or scenePath");

					const char* stagePath = data["stagePath"].asCString();
					const char* scenePath = data["scenePath"].asCString();

					// Read the .diastage JSON
					std::ifstream f(stagePath);
					if (!f.is_open())
						return MakeErrorResponse("cannot open .diastage file");

					std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
					f.close();

					Json::Value stageRoot;
					Json::CharReaderBuilder b;
					std::string parseErr;
					std::istringstream ss(content);
					if (!Json::parseFromStream(b, ss, &stageRoot, &parseErr))
						return MakeErrorResponse("failed to parse .diastage");

					// Compute relative scene path from the stage file's directory
					char stageDir[512] = {};
					const char* lastSep = nullptr;
					for (const char* p = stagePath; *p; ++p)
						if (*p == '/' || *p == '\\') lastSep = p;
					if (lastSep)
						strncpy_s(stageDir, sizeof(stageDir), stagePath, static_cast<size_t>(lastSep - stagePath + 1));

					// If scenePath starts with stageDir prefix, make it relative
					char relScene[512] = {};
					const size_t stageDirLen = strlen(stageDir);
					if (stageDirLen > 0 && strncmp(scenePath, stageDir, stageDirLen) == 0)
						strncpy_s(relScene, sizeof(relScene), scenePath + stageDirLen, _TRUNCATE);
					else
						strncpy_s(relScene, sizeof(relScene), scenePath, _TRUNCATE);

					// Normalise backslashes
					for (char* p = relScene; *p; ++p)
						if (*p == '\\') *p = '/';

					stageRoot["scene"] = relScene;

					// Write back
					std::ofstream out(stagePath);
					if (!out.is_open())
						return MakeErrorResponse("cannot write .diastage file");
					Json::StreamWriterBuilder wb;
					wb["indentCharacter"] = "    ";
					std::string outStr = Json::writeString(wb, stageRoot);
					out << outStr;
					out.close();

					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: associated scene '%s' to stage '%s'", relScene, stagePath);

					// Rebuild stage list and push to UI
					if (GetModel())
					{
						const Dia::Editor::ProjectContext& proj = GetModel()->GetDiagameProject();
						if (proj.IsValid())
						{
							mStageList = mProjectContextManager.BuildStageListJson(proj.diagamePath);
							if (GetBridge())
							{
								Json::Value payload(Json::objectValue);
								payload["diagamePath"] = proj.diagamePath;
								payload["isValid"]     = true;
								payload["stages"]      = mStageList;
								GetBridge()->NotifyUIDataChanged("scene_editor.project_changed", payload);
							}
						}
					}

					Json::Value result;
					result["success"] = true;
					return result;
				});
		}

		// ═══════════════════════════════════════════════════════════════════════
		// Extracted handler methods — testable without bridge/registration
		// ═══════════════════════════════════════════════════════════════════════

		Json::Value DiaSceneEditorPlugin::HandleAddItem(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.add_item", Dia::Observation::Trace::Category::kNone);
			if (!data.isMember("itemType") || !data.isMember("blueprintId"))
				return MakeErrorResponse("missing itemType or blueprintId");
			if (mLoadedSceneRoot.isNull())
				return MakeErrorResponse("no scene loaded");

			char err[256] = {};
			if (!SceneMutator::AddItem(mLoadedSceneRoot,
			        data["itemType"].asCString(),
			        data["blueprintId"].asCString(),
			        err, sizeof(err)))
				return MakeErrorResponse(err);

			if (GetBridge() && mSceneCatalogueId[0] != '\0')
			{
				Json::Value relReq;
				relReq["from"] = mSceneCatalogueId;
				relReq["rel"]  = "uses";
				relReq["to"]   = data["blueprintId"].asString();
				GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.add_relationship"), relReq);
			}

			MarkDirty();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
			return result;
		}

		Json::Value DiaSceneEditorPlugin::HandleDeleteItem(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.delete_item", Dia::Observation::Trace::Category::kNone);
			if (!data.isMember("itemType") || !data.isMember("itemId"))
				return MakeErrorResponse("missing itemType or itemId");
			if (mLoadedSceneRoot.isNull())
				return MakeErrorResponse("no scene loaded");

			char blueprintIdForRemove[256] = {};
			if (mSceneCatalogueId[0] != '\0')
			{
				const char* arrayKey = nullptr;
				const char* itemTypeStr = data["itemType"].asCString();
				if      (strcmp(itemTypeStr, "entity") == 0) arrayKey = "entities";
				else if (strcmp(itemTypeStr, "camera") == 0) arrayKey = "cameras";
				else if (strcmp(itemTypeStr, "light")  == 0) arrayKey = "lights";

				if (arrayKey && mLoadedSceneRoot.isMember("scene2d"))
				{
					const Json::Value& arr = mLoadedSceneRoot["scene2d"][arrayKey];
					const char* itemIdStr = data["itemId"].asCString();
					char idBuf[256];
					for (unsigned int i = 0; i < arr.size(); ++i)
					{
						if (!arr[i].isMember("id")) continue;
						const Json::Value& idVal = arr[i]["id"];
						const char* v = idVal.isString() ? idVal.asCString()
						              : (idVal.isObject() && idVal.isMember("value") ? idVal["value"].asCString() : "");
						strncpy_s(idBuf, sizeof(idBuf), v, _TRUNCATE);
						if (strcmp(idBuf, itemIdStr) == 0 && arr[i].isMember("blueprint"))
						{
							const Json::Value& bp = arr[i]["blueprint"];
							const char* bpVal = bp.isString() ? bp.asCString()
							                  : (bp.isObject() && bp.isMember("value") ? bp["value"].asCString() : "");
							strncpy_s(blueprintIdForRemove, sizeof(blueprintIdForRemove), bpVal, _TRUNCATE);
							break;
						}
					}
				}
			}

			char err[256] = {};
			if (!SceneMutator::DeleteItem(mLoadedSceneRoot,
			        data["itemType"].asCString(),
			        data["itemId"].asCString(),
			        err, sizeof(err)))
				return MakeErrorResponse(err);

			if (GetBridge() && mSceneCatalogueId[0] != '\0' && blueprintIdForRemove[0] != '\0')
			{
				Json::Value relReq;
				relReq["from"] = mSceneCatalogueId;
				relReq["rel"]  = "uses";
				relReq["to"]   = blueprintIdForRemove;
				GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.remove_relationship"), relReq);
			}

			mHierarchyController.ClearSelection();
			MarkDirty();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
			return result;
		}

		Json::Value DiaSceneEditorPlugin::HandleDuplicateItem(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.duplicate_item", Dia::Observation::Trace::Category::kNone);
			if (!data.isMember("itemType") || !data.isMember("itemId"))
				return MakeErrorResponse("missing itemType or itemId");
			if (mLoadedSceneRoot.isNull())
				return MakeErrorResponse("no scene loaded");

			char err[256] = {};
			if (!SceneMutator::DuplicateItem(mLoadedSceneRoot,
			        data["itemType"].asCString(),
			        data["itemId"].asCString(),
			        err, sizeof(err)))
				return MakeErrorResponse(err);

			MarkDirty();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
			return result;
		}

		Json::Value DiaSceneEditorPlugin::HandleRenameItem(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.rename_item", Dia::Observation::Trace::Category::kNone);
			if (!data.isMember("itemType") || !data.isMember("oldId") || !data.isMember("newId"))
				return MakeErrorResponse("missing itemType, oldId, or newId");
			if (mLoadedSceneRoot.isNull())
				return MakeErrorResponse("no scene loaded");

			char err[256] = {};
			if (!SceneMutator::RenameItem(mLoadedSceneRoot,
			        data["itemType"].asCString(),
			        data["oldId"].asCString(),
			        data["newId"].asCString(),
			        err, sizeof(err)))
				return MakeErrorResponse(err);

			mHierarchyController.SetSelection(
				data["itemType"].asCString(), data["newId"].asCString());
			MarkDirty();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = mHierarchyController.BuildHierarchyJson(mLoadedSceneRoot);
			return result;
		}

		Json::Value DiaSceneEditorPlugin::HandleLoadScene(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.load_scene", Dia::Observation::Trace::Category::kNone);
			if (!data.isMember("path") || !data["path"].isString())
				return MakeErrorResponse("missing path");

			Json::Value sceneRoot;
			char err[256] = {};
			if (!mFileHandler.Load(data["path"].asCString(), sceneRoot, err, sizeof(err)))
				return MakeErrorResponse(err[0] ? err : "load failed");

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: loaded scene '%s'", data["path"].asCString());
			mLoadedSceneRoot = sceneRoot;
			strncpy_s(mLoadedScenePath, sizeof(mLoadedScenePath), data["path"].asCString(), _TRUNCATE);
			mHierarchyController.ClearSelection();
			ClearDirty();
			ResolveCatalogueIdForLoadedScene();

			Json::Value result;
			result["success"]   = true;
			result["scene"]     = sceneRoot;
			result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
			result["dirty"]     = false;
			return result;
		}

		Json::Value DiaSceneEditorPlugin::HandleSaveScene(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.save_scene", Dia::Observation::Trace::Category::kNone);
			const char* savePath = mLoadedScenePath;
			if (data.isMember("path") && data["path"].isString())
				savePath = data["path"].asCString();

			if (!savePath || savePath[0] == '\0')
				return MakeErrorResponse("no path");

			const Json::Value& sceneToSave = data.isMember("scene")
				? data["scene"] : mLoadedSceneRoot;

			if (sceneToSave.isNull())
				return MakeErrorResponse("no scene data");

			char err[256] = {};
			if (!mFileHandler.Save(savePath, sceneToSave, err, sizeof(err)))
				return MakeErrorResponse(err[0] ? err : "save failed");

			mLoadedSceneRoot = sceneToSave;
			strncpy_s(mLoadedScenePath, sizeof(mLoadedScenePath), savePath, _TRUNCATE);
			ClearDirty();

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: saved scene '%s'", savePath);
			Json::Value result;
			result["success"] = true;
			result["dirty"]   = false;
			return result;
		}

		Json::Value DiaSceneEditorPlugin::HandleValidate(const Json::Value& /*data*/)
		{
			DIA_TRACE_ZONE("scene_editor.validate", Dia::Observation::Trace::Category::kNone);
			if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
			Json::Value result;
			result["success"]    = true;
			result["validation"] = mValidator.Validate(mLoadedSceneRoot);
			return result;
		}
	}
}
