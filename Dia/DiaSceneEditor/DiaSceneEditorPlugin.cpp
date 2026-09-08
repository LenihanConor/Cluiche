#include "DiaSceneEditor/DiaSceneEditorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/AppEditor/AppEditorController.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/Json/external/json/json.h>
#include <fstream>
#include <sstream>
#include <string>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>

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

			mDiagameDir[0] = '\0';
			if (ctx.IsValid() && ctx.diagamePath[0] != '\0')
			{
				strncpy_s(mDiagameDir, sizeof(mDiagameDir), ctx.diagamePath, _TRUNCATE);
				char* lastSep = nullptr;
				for (char* p = mDiagameDir; *p; ++p)
					if (*p == '/' || *p == '\\') lastSep = p;
				if (lastSep) *(lastSep + 1) = '\0';
				else mDiagameDir[0] = '\0';
			}

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
			DualRegisterActions();

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnPluginLoad complete");
		}

		void DiaSceneEditorPlugin::OnPluginUnload()
		{
			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: OnPluginUnload");
			if (GetServices() != nullptr)
			{
				Dia::Editor::EditorActionRegistryService* regSvc =
					GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
				if (regSvc != nullptr && regSvc->GetRegistry() != nullptr)
					regSvc->GetRegistry()->DeregisterActionsForOwner(
						Dia::Core::StringCRC("DiaSceneEditorPlugin"));
			}
			mLoadedSceneRoot = Json::Value();
			mLoadedScenePath[0] = '\0';
		}

		void DiaSceneEditorPlugin::OnNavigate(const Dia::Core::StringCRC& instanceId)
		{
			if (instanceId == Dia::Core::StringCRC::kZero)
				return;

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

			// Report to AppEditorController so app_editor.get_active_context reflects the open scene.
			if (GetServices() != nullptr)
			{
				Dia::Editor::AppEditorController* ctrl =
					GetServices()->GetService<Dia::Editor::AppEditorController>();
				if (ctrl != nullptr)
				{
					ctrl->SetEditTarget(
						Dia::Core::StringCRC("scene"),
						instanceId,
						sourcePath.c_str(),
						false);
					ctrl->SetFocus(Dia::Core::StringCRC("DiaSceneEditorPlugin"), "scene_editor");
				}
			}
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

		Json::Value DiaSceneEditorPlugin::ResolveTemplateCatalogueRecord(const char* templateName, const char* itemType) const
		{
			if (!templateName || templateName[0] == '\0' || !GetBridge())
				return Json::Value(Json::nullValue);

			// Determine the expected asset type from itemType
			const char* assetType = "diaentitytemplate";
			if (itemType && strcmp(itemType, "camera") == 0) assetType = "diacamera";
			else if (itemType && strcmp(itemType, "light") == 0) assetType = "dialight";

			Json::Value stateResult = GetBridge()->InvokeRequestHandler(
				Dia::Core::StringCRC("asset_catalogue.get_state"),
				Json::Value(Json::objectValue));

			if (stateResult.isNull() || !stateResult.isMember("records"))
				return Json::Value(Json::nullValue);

			// Build suffix to match: ".<templateName>"
			std::string suffix = std::string(".") + templateName;

			const Json::Value& records = stateResult["records"];
			for (unsigned int i = 0; i < records.size(); ++i)
			{
				const Json::Value& rec = records[i];
				if (rec.get("type", "").asString() != assetType)
					continue;

				std::string id = rec.get("id", "").asString();

				// Exact match (handles full catalogue IDs like "diaentitytemplate.hero")
				if (_stricmp(id.c_str(), templateName) == 0)
					return rec;

				// Match if catalogue id ends with ".<templateName>"
				if (id.size() >= suffix.size() &&
				    _stricmp(id.c_str() + id.size() - suffix.size(), suffix.c_str()) == 0)
					return rec;

				// Also match if source_path stem equals templateName
				std::string src = rec.get("source_path", "").asString();
				// Extract stem: last path component without extension
				size_t lastSep = src.find_last_of("/\\");
				std::string stem = (lastSep != std::string::npos) ? src.substr(lastSep + 1) : src;
				size_t dot = stem.rfind('.');
				if (dot != std::string::npos) stem = stem.substr(0, dot);
				if (_stricmp(stem.c_str(), templateName) == 0)
					return rec;
			}

			return Json::Value(Json::nullValue);
		}

		void DiaSceneEditorPlugin::EnrichTemplateKnown(Json::Value& hierarchy)
		{
			static const char* kSections[] = { "entities", "cameras", "lights" };
			static const char* kItemTypes[] = { "entity", "camera", "light" };

			for (int s = 0; s < 3; ++s)
			{
				if (!hierarchy.isMember(kSections[s])) continue;
				Json::Value& arr = hierarchy[kSections[s]];
				for (unsigned int i = 0; i < arr.size(); ++i)
				{
					Json::Value& item = arr[i];
					if (!item.isMember("entityTemplate")) continue;
					const char* etId = item["entityTemplate"].asCString();
					if (etId[0] == '\0')
					{
						item["entityTemplate_known"] = true;
						continue;
					}
					Json::Value rec = ResolveTemplateCatalogueRecord(etId, kItemTypes[s]);
					item["entityTemplate_known"] = !rec.isNull();
				}
			}
		}

		Json::Value DiaSceneEditorPlugin::BuildEnrichedHierarchy(const Json::Value& sceneRoot)
		{
			Json::Value h = mHierarchyController.BuildHierarchyJson(sceneRoot);
			EnrichTemplateKnown(h);
			return h;
		}

		Json::Value DiaSceneEditorPlugin::BuildEnrichedHierarchyFiltered(const Json::Value& sceneRoot, const char* filter)
		{
			Json::Value h = mHierarchyController.BuildFilteredHierarchyJson(sceneRoot, filter);
			EnrichTemplateKnown(h);
			return h;
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
					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: load_stage_scene requested — stagePath='%s' stageListSize=%u",
						stagePath, mStageList.size());
					Json::Value matchedStage;
					for (unsigned int i = 0; i < mStageList.size(); ++i)
					{
						DIA_LOG_INFO("Editor", "  mStageList[%u] stagePath='%s'", i,
							mStageList[i]["stagePath"].asCString());
						if (mStageList[i]["stagePath"].asString() == stagePath)
						{
							matchedStage = mStageList[i];
							break;
						}
					}

					if (matchedStage.isNull())
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: stage not found in list for stagePath='%s'", stagePath);
						return MakeErrorResponse("stage not in project");
					}

					const std::string scenePath = matchedStage["scenePath"].asString();
					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: matched stage — scenePath='%s'", scenePath.c_str());
					if (scenePath.empty())
					{
						DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: stage has no scene assigned");
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
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: failed to load scene '%s': %s", scenePath.c_str(), err);
						return MakeErrorResponse(err[0] ? err : "scene load failed");
					}

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
					result["hierarchy"] = BuildEnrichedHierarchy(sceneRoot);
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
					result["hierarchy"] = BuildEnrichedHierarchyFiltered(mLoadedSceneRoot, filter);
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

					// Propagate selection to AppEditorController.
					if (GetServices() != nullptr)
					{
						Dia::Editor::AppEditorController* ctrl =
							GetServices()->GetService<Dia::Editor::AppEditorController>();
						if (ctrl != nullptr)
						{
							ctrl->SetSelection(
								Dia::Core::StringCRC(data["type"].asCString()),
								Dia::Core::StringCRC(data["id"].asCString()),
								data.get("name", "").asCString());
						}
					}

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
					result["hierarchy"] = BuildEnrichedHierarchy(sceneRoot);
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

					// Resolve template base path from catalogue if possible, fall back to scene dir
					char entityTemplateBasePath[512] = {};
					const char* selType = data["selectionType"].asCString();
					const char* selId   = data["selectionId"].asCString();

					// Find the blueprint name for this item from the loaded scene
					const char* templateName = nullptr;
					char templateNameBuf[256] = {};
					const Json::Value& scene = mLoadedSceneRoot.isMember("scene2d") ? mLoadedSceneRoot["scene2d"] : Json::Value::null;
					auto extractBlueprintName = [&](const char* arrayKey) {
						if (!scene.isMember(arrayKey)) return;
						const Json::Value& arr = scene[arrayKey];
						for (unsigned int i = 0; i < arr.size(); ++i)
						{
							if (!arr[i].isMember("id")) continue;
							const Json::Value& idVal = arr[i]["id"];
							const char* v = idVal.isString() ? idVal.asCString()
							              : (idVal.isObject() && idVal.isMember("value") ? idVal["value"].asCString() : "");
							if (strcmp(v, selId) == 0 && arr[i].isMember("blueprint"))
							{
								const Json::Value& bp = arr[i]["blueprint"];
								const char* bpVal = bp.isString() ? bp.asCString()
								                  : (bp.isObject() && bp.isMember("value") ? bp["value"].asCString() : "");
								strncpy_s(templateNameBuf, sizeof(templateNameBuf), bpVal, _TRUNCATE);
								templateName = templateNameBuf;
								break;
							}
						}
					};

					if      (strcmp(selType, "entity") == 0) extractBlueprintName("entities");
					else if (strcmp(selType, "camera") == 0) extractBlueprintName("cameras");
					else if (strcmp(selType, "light")  == 0) extractBlueprintName("lights");

					if (templateName && templateName[0] != '\0')
					{
						Json::Value catRec = ResolveTemplateCatalogueRecord(templateName, selType);
						if (!catRec.isNull())
						{
							std::string srcPath = catRec.get("source_path", "").asString();
							for (char& c : srcPath) if (c == '\\') c = '/';
							size_t lastSlash = srcPath.rfind('/');
							std::string dir = (lastSlash != std::string::npos) ? srcPath.substr(0, lastSlash) : ".";

							// Prepend diagame dir if relative
							bool isAbs = (!dir.empty() && (dir[0] == '/' || (dir.size() > 1 && dir[1] == ':')));
							if (!isAbs && mDiagameDir[0] != '\0')
							{
								std::string full = std::string(mDiagameDir) + dir;
								strncpy_s(entityTemplateBasePath, sizeof(entityTemplateBasePath), full.c_str(), _TRUNCATE);
							}
							else
								strncpy_s(entityTemplateBasePath, sizeof(entityTemplateBasePath), dir.c_str(), _TRUNCATE);
						}
					}

					// Fall back to scene file directory
					if (entityTemplateBasePath[0] == '\0' && mLoadedScenePath[0] != '\0')
					{
						strncpy_s(entityTemplateBasePath, sizeof(entityTemplateBasePath), mLoadedScenePath, _TRUNCATE);
						for (char* p = entityTemplateBasePath; *p; ++p)
							if (*p == '\\') *p = '/';
						char* lastSlash = nullptr;
						for (char* p = entityTemplateBasePath; *p; ++p)
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
						entityTemplateBasePath);
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
				Dia::Core::StringCRC("scene_editor.get_entity_template_defaults"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_entity_template_defaults", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("entityTemplateId") || !data["entityTemplateId"].isString()
					    || !data.isMember("itemType")  || !data["itemType"].isString())
						return MakeErrorResponse("missing entityTemplateId or itemType");

					char entityTemplateBasePath[512] = {};
					{
						const char* etId = data["entityTemplateId"].asCString();
						const char* iType = data["itemType"].asCString();
						Json::Value catRec = ResolveTemplateCatalogueRecord(etId, iType);
						if (!catRec.isNull())
						{
							std::string srcPath = catRec.get("source_path", "").asString();
							for (char& c : srcPath) if (c == '\\') c = '/';
							size_t ls = srcPath.rfind('/');
							std::string dir = (ls != std::string::npos) ? srcPath.substr(0, ls) : ".";
							bool isAbs = (!dir.empty() && (dir[0] == '/' || (dir.size() > 1 && dir[1] == ':')));
							if (!isAbs && mDiagameDir[0] != '\0')
							{
								std::string full = std::string(mDiagameDir) + dir;
								strncpy_s(entityTemplateBasePath, sizeof(entityTemplateBasePath), full.c_str(), _TRUNCATE);
							}
							else
								strncpy_s(entityTemplateBasePath, sizeof(entityTemplateBasePath), dir.c_str(), _TRUNCATE);
						}
					}
					if (entityTemplateBasePath[0] == '\0' && mLoadedScenePath[0] != '\0')
					{
						strncpy_s(entityTemplateBasePath, sizeof(entityTemplateBasePath), mLoadedScenePath, _TRUNCATE);
						for (char* p = entityTemplateBasePath; *p; ++p)
							if (*p == '\\') *p = '/';
						char* lastSlash = nullptr;
						for (char* p = entityTemplateBasePath; *p; ++p)
							if (*p == '/') lastSlash = p;
						if (lastSlash) *lastSlash = '\0';
					}

					Json::Value defaults = mPropertyController.BuildBlueprintDefaultsJson(
						data["entityTemplateId"].asCString(),
						data["itemType"].asCString(),
						entityTemplateBasePath);

					Json::Value result;
					result["success"] = true;
					result["data"]    = defaults;
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.mark_dirty"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					AutoSave();
					Json::Value result;
					result["success"] = true;
					result["dirty"]   = false;
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
				Dia::Core::StringCRC("scene_editor.get_available_entity_templates"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.get_available_entity_templates", Dia::Observation::Trace::Category::kNone);
					const char* itemType = data.isMember("itemType") && data["itemType"].isString()
						? data["itemType"].asCString() : "entity";

					const char* assetType = "diaentitytemplate";
					if (strcmp(itemType, "camera") == 0) assetType = "diacamera";
					else if (strcmp(itemType, "light") == 0) assetType = "dialight";

					Json::Value entityTemplates(Json::arrayValue);
					if (GetBridge())
					{
						Json::Value query;
						query["typeId"] = assetType;
						query["limit"]  = 50;
						Json::Value qResult = GetBridge()->InvokeRequestHandler(
							Dia::Core::StringCRC("asset_catalogue.query_asset_ids"), query);
						if (qResult.isMember("ids") && qResult["ids"].isArray())
						{
							for (unsigned int i = 0; i < qResult["ids"].size(); ++i)
							{
								Json::Value entry(Json::objectValue);
								entry["id"] = qResult["ids"][i];
								entityTemplates.append(entry);
							}
						}
					}

					Json::Value result;
					result["success"]         = true;
					result["assetType"]       = assetType;
					result["entityTemplates"] = entityTemplates;
					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: get_available_entity_templates type='%s' count=%u",
						itemType, entityTemplates.size());
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

					AutoSave();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.rename_item"),
				[this](const Json::Value& data) -> Json::Value { return HandleRenameItem(data); });

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.analyse_change_entity_template"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.analyse_change_entity_template", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType") || !data.isMember("itemId") || !data.isMember("newEntityTemplateId"))
						return MakeErrorResponse("missing required fields");
					if (mLoadedSceneRoot.isNull())
						return MakeErrorResponse("no scene loaded");

					Json::Value components = mPropertyController.LoadBlueprintComponents(
						data["newEntityTemplateId"].asCString(), mLoadedScenePath, data["itemType"].asCString());

					Json::Value result;
					result["success"]  = true;
					result["analysis"] = SceneMutator::AnalyseChangeBlueprintJson(
						mLoadedSceneRoot,
						data["itemType"].asCString(),
						data["itemId"].asCString(),
						components);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.change_entity_template"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.change_entity_template", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("itemType") || !data.isMember("itemId") || !data.isMember("newEntityTemplateId"))
						return MakeErrorResponse("missing required fields");
					if (mLoadedSceneRoot.isNull())
						return MakeErrorResponse("no scene loaded");

					Json::Value newComponents = mPropertyController.LoadBlueprintComponents(
						data["newEntityTemplateId"].asCString(), mLoadedScenePath, data["itemType"].asCString());

					char oldEntityTemplateId[256] = {};
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
									const char* etBuf = bp.isString() ? bp.asCString()
									                  : (bp.isObject() && bp.isMember("value") ? bp["value"].asCString() : "");
									strncpy_s(oldEntityTemplateId, sizeof(oldEntityTemplateId), etBuf, _TRUNCATE);
									break;
								}
							}
						}
					}

					char err[256] = {};
					if (!SceneMutator::ChangeBlueprint(mLoadedSceneRoot,
					        data["itemType"].asCString(), data["itemId"].asCString(),
					        data["newEntityTemplateId"].asCString(), newComponents, err, sizeof(err)))
						return MakeErrorResponse(err);

					if (GetBridge() && mSceneCatalogueId[0] != '\0')
					{
						if (oldEntityTemplateId[0] != '\0')
						{
							Json::Value removeReq;
							removeReq["from"] = mSceneCatalogueId;
							removeReq["rel"]  = "uses";
							removeReq["to"]   = oldEntityTemplateId;
							GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.remove_relationship"), removeReq);
						}
						Json::Value addReq;
						addReq["from"] = mSceneCatalogueId;
						addReq["rel"]  = "uses";
						addReq["to"]   = data["newEntityTemplateId"].asString();
						GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.add_relationship"), addReq);
					}

					AutoSave();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
					return result;
				});

			// ── Layer CRUD ──────────────────────────────────────────────────────

			auto layerMutate = [this](bool ok, const char* err) -> Json::Value
			{
				if (!ok) return MakeErrorResponse(err);
				AutoSave();
				Json::Value result;
				result["success"]   = true;
				result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
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
					AutoSave();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
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
					AutoSave();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.set_light_type"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.set_light_type", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("lightId") || !data.isMember("type")) return MakeErrorResponse("missing lightId or type");
					if (mLoadedSceneRoot.isNull()) return MakeErrorResponse("no scene loaded");
					char err[256]={};
					if (!SceneMutator::SetLightType(mLoadedSceneRoot, data["lightId"].asCString(), data["type"].asCString(), err, sizeof(err)))
						return MakeErrorResponse(err);
					AutoSave();
					Json::Value result;
					result["success"]   = true;
					result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
					return result;
				});

			// ── Override management ──────────────────────────────────────────────

			auto overrideMutate = [this](bool ok, const char* err) -> Json::Value
			{
				if (!ok) return MakeErrorResponse(err);
				AutoSave();
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
					AutoSave();
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("scene_editor.new_scene_shortcut"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.new_scene_shortcut", Dia::Observation::Trace::Category::kNone);
					if (GetPluginLoader())
						GetPluginLoader()->LoadPlugin(
							Dia::Core::StringCRC("DiaAssetCatalogueEditorPlugin"),
							Dia::Core::StringCRC("DiaAssetCatalogueEditorPlugin"));

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
							Dia::Core::StringCRC("DiaAssetCatalogueEditorPlugin"),
							Dia::Core::StringCRC("DiaAssetCatalogueEditorPlugin"));

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
				Dia::Core::StringCRC("scene_editor.open_entity_template"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.open_entity_template", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("entityTemplateId") || !data["entityTemplateId"].isString())
						return MakeErrorResponse("missing entityTemplateId");

					const char* bareName = data["entityTemplateId"].asCString();
					const char* itemType = data.isMember("itemType") ? data["itemType"].asCString() : "entity";

					// Resolve bare template name to catalogue id
					std::string catalogueId = bareName;
					Json::Value catRec = ResolveTemplateCatalogueRecord(bareName, itemType);
					if (!catRec.isNull())
						catalogueId = catRec.get("id", bareName).asString();

					const Dia::Core::StringCRC templateEditorType("DiaEntityTemplateEditor");
					const Dia::Core::StringCRC resolvedId(catalogueId.c_str());
					if (GetPluginLoader())
						GetPluginLoader()->LoadPlugin(templateEditorType, resolvedId);

					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: open_entity_template '%s' -> catalogueId='%s'",
						bareName, catalogueId.c_str());
					return MakeSuccessResponse();
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
		// DualRegisterActions — mirror WebUIBridge handlers into EditorActionRegistry
		// ═══════════════════════════════════════════════════════════════════════

		void DiaSceneEditorPlugin::DualRegisterActions()
		{
			if (GetServices() == nullptr) return;
			Dia::Editor::EditorActionRegistryService* regSvc =
				GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
			if (regSvc == nullptr || regSvc->GetRegistry() == nullptr) return;
			Dia::Editor::EditorActionRegistry* api = regSvc->GetRegistry();

			RegisterFileActions(api);
			RegisterHierarchyCRUDActions(api);
			RegisterLayerActions(api);
			RegisterOverrideActions(api);
			RegisterTemplateActions(api);
			RegisterCrossPluginActions(api);

			DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: Dual-registered 40 scene_editor.* actions");
		}

		void DiaSceneEditorPlugin::RegisterFileActions(Dia::Editor::EditorActionRegistry* api)
		{
			using DT = Dia::Editor::DispatchThread;

			auto reg = [&](const char* name, const char* description,
			               Dia::Editor::DispatchThread dispatch,
			               Dia::Editor::EditorActionParam* paramsArr = nullptr,
			               unsigned int paramCount = 0)
			{
				Dia::Core::StringCRC handlerKey(name);
				Dia::Editor::EditorActionDescriptor d;
				d.name           = handlerKey;
				d.description    = description;
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = dispatch;
				d.handler        = [this, handlerKey](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					return GetBridge()->InvokeRequestHandler(handlerKey, data);
				};
				for (unsigned int i = 0; i < paramCount; ++i)
					d.params.params.Add(paramsArr[i]);
				api->RegisterAction(d);
			};

			// get_project_state
			reg("scene_editor.get_project_state",
			    "Returns the current scene editor project state including whether a .diagame project is loaded and its path.",
			    DT::kCallerThread);

			// get_stage_list
			reg("scene_editor.get_stage_list",
			    "Returns the list of stages defined in the currently loaded .diagame project.",
			    DT::kCallerThread);

			// load_stage_scene
			{
				Dia::Editor::EditorActionParam p;
				p.name="stagePath"; p.type="string"; p.required=true;
				p.description="Path to the .diastage file.";
				reg("scene_editor.load_stage_scene",
				    "Loads the scene associated with the specified stage into the scene editor.",
				    DT::kMainThread, &p, 1);
			}

			// load_scene
			{
				Dia::Editor::EditorActionParam p;
				p.name="path"; p.type="string"; p.required=true;
				p.description="Absolute path to the .diascene file.";
				reg("scene_editor.load_scene",
				    "Loads a .diascene file directly by absolute path into the scene editor.",
				    DT::kMainThread, &p, 1);
			}

			// save_scene
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="path";  params[0].type="string"; params[0].required=false; params[0].description="Optional save path override.";
				params[1].name="scene"; params[1].type="object"; params[1].required=false; params[1].description="Optional scene JSON to save.";
				reg("scene_editor.save_scene",
				    "Saves the currently loaded scene to disk. Optionally accepts a path or scene override.",
				    DT::kMainThread, params, 2);
			}

			// get_dirty_state
			reg("scene_editor.get_dirty_state",
			    "Returns whether the currently loaded scene has unsaved changes.",
			    DT::kCallerThread);
		}

		void DiaSceneEditorPlugin::RegisterHierarchyCRUDActions(Dia::Editor::EditorActionRegistry* api)
		{
			using DT = Dia::Editor::DispatchThread;

			auto reg = [&](const char* name, const char* description,
			               Dia::Editor::DispatchThread dispatch,
			               Dia::Editor::EditorActionParam* paramsArr = nullptr,
			               unsigned int paramCount = 0)
			{
				Dia::Core::StringCRC handlerKey(name);
				Dia::Editor::EditorActionDescriptor d;
				d.name           = handlerKey;
				d.description    = description;
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = dispatch;
				d.handler        = [this, handlerKey](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					return GetBridge()->InvokeRequestHandler(handlerKey, data);
				};
				for (unsigned int i = 0; i < paramCount; ++i)
					d.params.params.Add(paramsArr[i]);
				api->RegisterAction(d);
			};

			// get_hierarchy
			{
				Dia::Editor::EditorActionParam p;
				p.name="path"; p.type="string"; p.required=true;
				p.description="Path to the .diascene file to load hierarchy from.";
				reg("scene_editor.get_hierarchy",
				    "Loads and returns the enriched hierarchy tree for a .diascene file.",
				    DT::kCallerThread, &p, 1);
			}

			// get_hierarchy_filtered
			{
				Dia::Editor::EditorActionParam p;
				p.name="filter"; p.type="string"; p.required=false;
				p.description="Filter string to narrow hierarchy results.";
				reg("scene_editor.get_hierarchy_filtered",
				    "Returns the hierarchy of the currently loaded scene filtered by the given string.",
				    DT::kCallerThread, &p, 1);
			}

			// get_properties
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="selectionType"; params[0].type="string"; params[0].required=true; params[0].description="Item type: 'entity', 'camera', or 'light'.";
				params[1].name="selectionId";   params[1].type="string"; params[1].required=true; params[1].description="The item ID.";
				reg("scene_editor.get_properties",
				    "Returns property data for the selected scene item (entity, camera, or light).",
				    DT::kCallerThread, params, 2);
			}

			// set_selection
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="type"; params[0].type="string"; params[0].required=false; params[0].description="Item type to select.";
				params[1].name="id";   params[1].type="string"; params[1].required=false; params[1].description="Item ID to select.";
				reg("scene_editor.set_selection",
				    "Sets the current selection in the hierarchy to the specified item type and ID.",
				    DT::kMainThread, params, 2);
			}

			// add_item
			{
				Dia::Editor::EditorActionParam params[3];
				params[0].name="itemType";         params[0].type="string"; params[0].required=true;  params[0].description="Item type: 'entity', 'camera', or 'light'.";
				params[1].name="entityTemplateId"; params[1].type="string"; params[1].required=true;  params[1].description="Template asset ID to instantiate.";
				params[2].name="id";               params[2].type="string"; params[2].required=false; params[2].description="Optional explicit item ID.";
				reg("scene_editor.add_item",
				    "Adds a new item (entity, camera, or light) to the loaded scene using the given template.",
				    DT::kMainThread, params, 3);
			}

			// duplicate_item
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="itemType"; params[0].type="string"; params[0].required=true; params[0].description="Item type to duplicate.";
				params[1].name="itemId";   params[1].type="string"; params[1].required=true; params[1].description="ID of the item to duplicate.";
				reg("scene_editor.duplicate_item",
				    "Duplicates an existing item in the scene, creating a copy with a new ID.",
				    DT::kMainThread, params, 2);
			}

			// delete_item
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="itemType"; params[0].type="string"; params[0].required=true; params[0].description="Item type to delete.";
				params[1].name="itemId";   params[1].type="string"; params[1].required=true; params[1].description="ID of the item to delete.";
				reg("scene_editor.delete_item",
				    "Deletes an item (entity, camera, or light) from the loaded scene by ID.",
				    DT::kMainThread, params, 2);
			}

			// rename_item
			{
				Dia::Editor::EditorActionParam params[3];
				params[0].name="itemType"; params[0].type="string"; params[0].required=true; params[0].description="Item type to rename.";
				params[1].name="oldId";    params[1].type="string"; params[1].required=true; params[1].description="Current item ID.";
				params[2].name="newId";    params[2].type="string"; params[2].required=true; params[2].description="New item ID.";
				reg("scene_editor.rename_item",
				    "Renames an item in the loaded scene by changing its ID.",
				    DT::kMainThread, params, 3);
			}

			// set_enabled
			{
				Dia::Editor::EditorActionParam params[3];
				params[0].name="itemType"; params[0].type="string"; params[0].required=true; params[0].description="Item type.";
				params[1].name="itemId";   params[1].type="string"; params[1].required=true; params[1].description="Item ID.";
				params[2].name="enabled";  params[2].type="bool";   params[2].required=true; params[2].description="True to enable, false to disable.";
				reg("scene_editor.set_enabled",
				    "Enables or disables an item in the loaded scene.",
				    DT::kMainThread, params, 3);
			}

			// mark_dirty
			reg("scene_editor.mark_dirty",
			    "Marks the scene as dirty and triggers an autosave.",
			    DT::kMainThread);

			// validate
			reg("scene_editor.validate",
			    "Validates the currently loaded scene and returns validation results.",
			    DT::kCallerThread);

			// get_scene_properties
			reg("scene_editor.get_scene_properties",
			    "Returns scene-level properties including world bounds, counts of entities/cameras/lights/layers, and validation status.",
			    DT::kCallerThread);

			// set_world_bounds
			{
				Dia::Editor::EditorActionParam p;
				p.name="world_bounds"; p.type="object"; p.required=true;
				p.description="World bounds object to set on the scene.";
				reg("scene_editor.set_world_bounds",
				    "Sets the world bounds of the currently loaded scene.",
				    DT::kMainThread, &p, 1);
			}

			// set_camera_active
			{
				Dia::Editor::EditorActionParam p;
				p.name="cameraId"; p.type="string"; p.required=true;
				p.description="ID of the camera to make active.";
				reg("scene_editor.set_camera_active",
				    "Sets the specified camera as the active camera in the loaded scene.",
				    DT::kMainThread, &p, 1);
			}

			// get_entities (new action — custom handler)
			{
				Dia::Editor::EditorActionDescriptor d;
				d.name           = Dia::Core::StringCRC("scene_editor.get_entities");
				d.description    = "Returns a flat list of all entity entries in the currently loaded scene. "
				                   "Each entry includes id, templateId, enabled, layerId, and overrides. "
				                   "Use type param to filter: 'entity', 'camera', or 'light'.";
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = DT::kCallerThread;
				{
					Dia::Editor::EditorActionParam p;
					p.name="type"; p.type="string"; p.required=false;
					p.description="Item type to filter: 'entity', 'camera', or 'light'. Omit for all.";
					d.params.params.Add(p);
				}
				d.handler = [this](const Json::Value& data) -> Json::Value {
					if (mLoadedSceneRoot.isNull())
					{
						Json::Value r; r["success"]=true; r["entities"]=Json::Value(Json::arrayValue); return r;
					}
					const char* typeFilter = (data.isMember("type") && data["type"].isString())
					                       ? data["type"].asCString() : nullptr;
					const Json::Value& scene = mLoadedSceneRoot.isMember("scene2d")
					                         ? mLoadedSceneRoot["scene2d"] : Json::Value::null;
					Json::Value entities(Json::arrayValue);
					static const char* kSections[] = { "entities", "cameras", "lights" };
					for (int s = 0; s < 3; ++s)
					{
						const char* sectionType = (s == 0) ? "entity" : (s == 1) ? "camera" : "light";
						if (typeFilter != nullptr && strcmp(typeFilter, sectionType) != 0) continue;
						if (scene.isNull() || !scene.isMember(kSections[s])) continue;
						const Json::Value& arr = scene[kSections[s]];
						for (unsigned int i = 0; i < arr.size(); ++i)
						{
							const Json::Value& item = arr[i];
							Json::Value entry(Json::objectValue);
							// id
							if (item.isMember("id"))
							{
								const Json::Value& idVal = item["id"];
								entry["id"] = idVal.isString() ? idVal : (idVal.isObject() && idVal.isMember("value") ? idVal["value"] : Json::Value(""));
							}
							else entry["id"] = "";
							// templateId (from blueprint field)
							if (item.isMember("blueprint"))
							{
								const Json::Value& bp = item["blueprint"];
								entry["templateId"] = bp.isString() ? bp : (bp.isObject() && bp.isMember("value") ? bp["value"] : Json::Value(""));
							}
							else entry["templateId"] = "";
							// enabled
							entry["enabled"] = item.isMember("enabled") ? item["enabled"].asBool() : true;
							// layerId
							if (item.isMember("layer_id"))
								entry["layerId"] = item["layer_id"];
							else if (item.isMember("layerId"))
								entry["layerId"] = item["layerId"];
							else
								entry["layerId"] = "";
							// overrides (instance_data)
							entry["overrides"] = item.isMember("instance_data") ? item["instance_data"] : Json::Value(Json::objectValue);
							entities.append(entry);
						}
					}
					Json::Value r; r["success"]=true; r["entities"]=entities; return r;
				};
				api->RegisterAction(d);
			}
		}

		void DiaSceneEditorPlugin::RegisterLayerActions(Dia::Editor::EditorActionRegistry* api)
		{
			using DT = Dia::Editor::DispatchThread;

			auto reg = [&](const char* name, const char* description,
			               Dia::Editor::DispatchThread dispatch,
			               Dia::Editor::EditorActionParam* paramsArr = nullptr,
			               unsigned int paramCount = 0)
			{
				Dia::Core::StringCRC handlerKey(name);
				Dia::Editor::EditorActionDescriptor d;
				d.name           = handlerKey;
				d.description    = description;
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = dispatch;
				d.handler        = [this, handlerKey](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					return GetBridge()->InvokeRequestHandler(handlerKey, data);
				};
				for (unsigned int i = 0; i < paramCount; ++i)
					d.params.params.Add(paramsArr[i]);
				api->RegisterAction(d);
			};

			// add_layer
			{
				Dia::Editor::EditorActionParam p;
				p.name="layerId"; p.type="string"; p.required=true;
				p.description="ID of the new layer to add.";
				reg("scene_editor.add_layer",
				    "Adds a new layer to the currently loaded scene.",
				    DT::kMainThread, &p, 1);
			}

			// delete_layer
			{
				Dia::Editor::EditorActionParam p;
				p.name="layerId"; p.type="string"; p.required=true;
				p.description="ID of the layer to delete.";
				reg("scene_editor.delete_layer",
				    "Deletes a layer from the currently loaded scene.",
				    DT::kMainThread, &p, 1);
			}

			// reorder_layer
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="layerId";   params[0].type="string"; params[0].required=true; params[0].description="ID of the layer to reorder.";
				params[1].name="newIndex";  params[1].type="int";    params[1].required=true; params[1].description="New zero-based index for the layer.";
				reg("scene_editor.reorder_layer",
				    "Moves a layer to a new position in the layer stack.",
				    DT::kMainThread, params, 2);
			}

			// update_layer
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="layerId"; params[0].type="string"; params[0].required=true; params[0].description="ID of the layer to update.";
				params[1].name="fields";  params[1].type="object"; params[1].required=true; params[1].description="Fields to update on the layer.";
				reg("scene_editor.update_layer",
				    "Updates fields on an existing layer in the loaded scene.",
				    DT::kMainThread, params, 2);
			}
		}

		void DiaSceneEditorPlugin::RegisterOverrideActions(Dia::Editor::EditorActionRegistry* api)
		{
			using DT = Dia::Editor::DispatchThread;

			auto reg = [&](const char* name, const char* description,
			               Dia::Editor::DispatchThread dispatch,
			               Dia::Editor::EditorActionParam* paramsArr = nullptr,
			               unsigned int paramCount = 0)
			{
				Dia::Core::StringCRC handlerKey(name);
				Dia::Editor::EditorActionDescriptor d;
				d.name           = handlerKey;
				d.description    = description;
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = dispatch;
				d.handler        = [this, handlerKey](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					return GetBridge()->InvokeRequestHandler(handlerKey, data);
				};
				for (unsigned int i = 0; i < paramCount; ++i)
					d.params.params.Add(paramsArr[i]);
				api->RegisterAction(d);
			};

			// add_override
			{
				Dia::Editor::EditorActionParam params[4];
				params[0].name="itemType";     params[0].type="string"; params[0].required=true;  params[0].description="Item type: 'entity', 'camera', or 'light'.";
				params[1].name="itemId";       params[1].type="string"; params[1].required=true;  params[1].description="Item ID to add the override to.";
				params[2].name="overrideKey";  params[2].type="string"; params[2].required=true;  params[2].description="Override field path key.";
				params[3].name="defaultValue"; params[3].type="any";    params[3].required=true;  params[3].description="Default value for the override.";
				reg("scene_editor.add_override",
				    "Adds an instance_data override to a scene item.",
				    DT::kMainThread, params, 4);
			}

			// remove_override
			{
				Dia::Editor::EditorActionParam params[3];
				params[0].name="itemType";    params[0].type="string"; params[0].required=true; params[0].description="Item type.";
				params[1].name="itemId";      params[1].type="string"; params[1].required=true; params[1].description="Item ID.";
				params[2].name="overrideKey"; params[2].type="string"; params[2].required=true; params[2].description="Override key to remove.";
				reg("scene_editor.remove_override",
				    "Removes an instance_data override from a scene item.",
				    DT::kMainThread, params, 3);
			}

			// update_override
			{
				Dia::Editor::EditorActionParam params[4];
				params[0].name="itemType";    params[0].type="string"; params[0].required=true; params[0].description="Item type.";
				params[1].name="itemId";      params[1].type="string"; params[1].required=true; params[1].description="Item ID.";
				params[2].name="overrideKey"; params[2].type="string"; params[2].required=true; params[2].description="Override key to update.";
				params[3].name="value";       params[3].type="any";    params[3].required=true; params[3].description="New value for the override.";
				reg("scene_editor.update_override",
				    "Updates the value of an existing instance_data override on a scene item.",
				    DT::kMainThread, params, 4);
			}
		}

		void DiaSceneEditorPlugin::RegisterTemplateActions(Dia::Editor::EditorActionRegistry* api)
		{
			using DT = Dia::Editor::DispatchThread;

			auto reg = [&](const char* name, const char* description,
			               Dia::Editor::DispatchThread dispatch,
			               Dia::Editor::EditorActionParam* paramsArr = nullptr,
			               unsigned int paramCount = 0)
			{
				Dia::Core::StringCRC handlerKey(name);
				Dia::Editor::EditorActionDescriptor d;
				d.name           = handlerKey;
				d.description    = description;
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = dispatch;
				d.handler        = [this, handlerKey](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					return GetBridge()->InvokeRequestHandler(handlerKey, data);
				};
				for (unsigned int i = 0; i < paramCount; ++i)
					d.params.params.Add(paramsArr[i]);
				api->RegisterAction(d);
			};

			// get_entity_template_defaults
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="entityTemplateId"; params[0].type="string"; params[0].required=true; params[0].description="Entity template asset ID.";
				params[1].name="itemType";         params[1].type="string"; params[1].required=true; params[1].description="Item type: 'entity', 'camera', or 'light'.";
				reg("scene_editor.get_entity_template_defaults",
				    "Returns the default component values for the given entity template.",
				    DT::kCallerThread, params, 2);
			}

			// get_available_entity_templates
			{
				Dia::Editor::EditorActionParam p;
				p.name="itemType"; p.type="string"; p.required=false;
				p.description="Item type to filter templates by (entity, camera, light).";
				reg("scene_editor.get_available_entity_templates",
				    "Returns a list of available entity templates from the asset catalogue, optionally filtered by item type.",
				    DT::kCallerThread, &p, 1);
			}

			// analyse_change_entity_template
			{
				Dia::Editor::EditorActionParam params[3];
				params[0].name="itemType";           params[0].type="string"; params[0].required=true; params[0].description="Item type.";
				params[1].name="itemId";             params[1].type="string"; params[1].required=true; params[1].description="Item ID.";
				params[2].name="newEntityTemplateId"; params[2].type="string"; params[2].required=true; params[2].description="New template ID to analyse changing to.";
				reg("scene_editor.analyse_change_entity_template",
				    "Analyses the impact of changing an item's entity template without applying the change.",
				    DT::kCallerThread, params, 3);
			}

			// change_entity_template
			{
				Dia::Editor::EditorActionParam params[3];
				params[0].name="itemType";           params[0].type="string"; params[0].required=true; params[0].description="Item type.";
				params[1].name="itemId";             params[1].type="string"; params[1].required=true; params[1].description="Item ID.";
				params[2].name="newEntityTemplateId"; params[2].type="string"; params[2].required=true; params[2].description="New template ID to change to.";
				reg("scene_editor.change_entity_template",
				    "Changes the entity template for a scene item and applies the resulting overrides.",
				    DT::kMainThread, params, 3);
			}
		}

		void DiaSceneEditorPlugin::RegisterCrossPluginActions(Dia::Editor::EditorActionRegistry* api)
		{
			using DT = Dia::Editor::DispatchThread;

			auto reg = [&](const char* name, const char* description,
			               Dia::Editor::DispatchThread dispatch,
			               Dia::Editor::EditorActionParam* paramsArr = nullptr,
			               unsigned int paramCount = 0)
			{
				Dia::Core::StringCRC handlerKey(name);
				Dia::Editor::EditorActionDescriptor d;
				d.name           = handlerKey;
				d.description    = description;
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = dispatch;
				d.handler        = [this, handlerKey](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					return GetBridge()->InvokeRequestHandler(handlerKey, data);
				};
				for (unsigned int i = 0; i < paramCount; ++i)
					d.params.params.Add(paramsArr[i]);
				api->RegisterAction(d);
			};

			// set_light_affects_layers
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="lightId";  params[0].type="string";   params[0].required=true; params[0].description="ID of the light.";
				params[1].name="layerIds"; params[1].type="string[]";  params[1].required=true; params[1].description="Array of layer IDs the light should affect.";
				reg("scene_editor.set_light_affects_layers",
				    "Sets which layers a light affects in the loaded scene.",
				    DT::kMainThread, params, 2);
			}

			// set_light_type
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="lightId"; params[0].type="string"; params[0].required=true; params[0].description="ID of the light.";
				params[1].name="type";    params[1].type="string"; params[1].required=true; params[1].description="Light type string.";
				reg("scene_editor.set_light_type",
				    "Sets the type of a light in the loaded scene.",
				    DT::kMainThread, params, 2);
			}

			// open_entity_template
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="entityTemplateId"; params[0].type="string"; params[0].required=true;  params[0].description="Entity template asset ID to open.";
				params[1].name="itemType";         params[1].type="string"; params[1].required=false; params[1].description="Item type hint for resolving the template.";
				reg("scene_editor.open_entity_template",
				    "Opens an entity template in the entity template editor plugin.",
				    DT::kMainThread, params, 2);
			}

			// associate_scene_to_stage
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name="stagePath"; params[0].type="string"; params[0].required=true; params[0].description="Absolute path to the .diastage file.";
				params[1].name="scenePath"; params[1].type="string"; params[1].required=true; params[1].description="Absolute path to the .diascene file.";
				reg("scene_editor.associate_scene_to_stage",
				    "Associates a .diascene file with a .diastage file by writing the relative path into the stage.",
				    DT::kMainThread, params, 2);
			}

			// place_entity (new action — custom handler)
			{
				Dia::Editor::EditorActionDescriptor d;
				d.name           = Dia::Core::StringCRC("scene_editor.place_entity");
				d.description    = "High-level: adds an entity to the scene with optional position and overrides in one call. "
				                   "Combines add_item and update_override. Auto-saves after inserting.";
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = DT::kMainThread;
				{
					Dia::Editor::EditorActionParam params[5];
					params[0].name="templateId"; params[0].type="string"; params[0].required=true;  params[0].description="Entity template asset ID.";
					params[1].name="position";   params[1].type="object"; params[1].required=false; params[1].description="Position override, e.g. { 'x': 100, 'y': 200 }.";
					params[2].name="overrides";  params[2].type="object"; params[2].required=false; params[2].description="Additional instance_data overrides as { fieldPath: value } pairs.";
					params[3].name="layerId";    params[3].type="string"; params[3].required=false; params[3].description="Layer to place into.";
					params[4].name="id";         params[4].type="string"; params[4].required=false; params[4].description="Explicit entity ID. Auto-generated if omitted.";
					for (int pi = 0; pi < 5; ++pi) d.params.params.Add(params[pi]);
				}
				d.handler = [this](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					// Step 1: add_item
					Json::Value addReq(Json::objectValue);
					addReq["itemType"] = "entity";
					if (data.isMember("templateId")) addReq["entityTemplateId"] = data["templateId"];
					if (data.isMember("id"))         addReq["id"]               = data["id"];
					if (data.isMember("layerId"))    addReq["layerId"]           = data["layerId"];
					Json::Value addResult = HandleAddItem(addReq);
					if (!addResult.get("success", false).asBool())
						return addResult;
					// Extract entity ID from hierarchy result
					std::string entityId;
					if (data.isMember("id") && data["id"].isString())
						entityId = data["id"].asString();
					else
					{
						// Find the newly added entity — it should be the last in the entities list
						const Json::Value& scene = mLoadedSceneRoot.isMember("scene2d")
						                         ? mLoadedSceneRoot["scene2d"] : Json::Value::null;
						if (!scene.isNull() && scene.isMember("entities"))
						{
							const Json::Value& arr = scene["entities"];
							if (arr.size() > 0)
							{
								const Json::Value& last = arr[arr.size() - 1];
								if (last.isMember("id"))
								{
									const Json::Value& idVal = last["id"];
									entityId = idVal.isString() ? idVal.asString()
									         : (idVal.isObject() && idVal.isMember("value") ? idVal["value"].asString() : "");
								}
							}
						}
					}
					// Step 2: apply position override if provided
					if (data.isMember("position") && data["position"].isObject() && !entityId.empty())
					{
						const Json::Value& pos = data["position"];
						Json::Value::Members keys = pos.getMemberNames();
						for (unsigned int ki = 0; ki < keys.size(); ++ki)
						{
							const std::string& fieldPath = keys[ki];
							Json::Value addOvReq(Json::objectValue);
							addOvReq["itemType"]     = "entity";
							addOvReq["itemId"]       = entityId;
							addOvReq["overrideKey"]  = std::string("Transform.position.") + fieldPath;
							addOvReq["defaultValue"] = pos[fieldPath];
							GetBridge()->InvokeRequestHandler(
								Dia::Core::StringCRC("scene_editor.add_override"), addOvReq);
						}
					}
					// Step 3: apply additional overrides if provided
					if (data.isMember("overrides") && data["overrides"].isObject() && !entityId.empty())
					{
						const Json::Value& ovs = data["overrides"];
						Json::Value::Members ovKeys = ovs.getMemberNames();
						for (unsigned int oki = 0; oki < ovKeys.size(); ++oki)
						{
							const std::string& fieldPath = ovKeys[oki];
							Json::Value addOvReq(Json::objectValue);
							addOvReq["itemType"]     = "entity";
							addOvReq["itemId"]       = entityId;
							addOvReq["overrideKey"]  = fieldPath;
							addOvReq["defaultValue"] = ovs[fieldPath];
							GetBridge()->InvokeRequestHandler(
								Dia::Core::StringCRC("scene_editor.add_override"), addOvReq);
						}
					}
					Json::Value r;
					r["success"]   = true;
					r["entityId"]  = entityId;
					r["hierarchy"] = addResult.isMember("hierarchy") ? addResult["hierarchy"] : Json::Value(Json::objectValue);
					return r;
				};
				api->RegisterAction(d);
			}

			// remove_entity (new action — custom handler)
			{
				Dia::Editor::EditorActionDescriptor d;
				d.name           = Dia::Core::StringCRC("scene_editor.remove_entity");
				d.description    = "Removes an entity from the scene by ID. Convenience wrapper around delete_item with itemType='entity'. Auto-saves.";
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = DT::kMainThread;
				{
					Dia::Editor::EditorActionParam p;
					p.name="entityId"; p.type="string"; p.required=true;
					p.description="ID of the entity to remove.";
					d.params.params.Add(p);
				}
				d.handler = [this](const Json::Value& data) -> Json::Value {
					Json::Value req(Json::objectValue);
					req["itemType"] = "entity";
					req["itemId"]   = data.isMember("entityId") ? data["entityId"] : Json::Value("");
					return HandleDeleteItem(req);
				};
				api->RegisterAction(d);
			}

			// create_scene (new action — custom handler)
			{
				Dia::Editor::EditorActionDescriptor d;
				d.name           = Dia::Core::StringCRC("scene_editor.create_scene");
				d.description    = "Creates a new scene asset record and stub .diascene file without opening any dialog, "
				                   "then immediately loads the new scene into the scene editor.";
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = DT::kMainThread;
				{
					Dia::Editor::EditorActionParam params[3];
					params[0].name="id";          params[0].type="string";   params[0].required=true;  params[0].description="Unique scene asset ID.";
					params[1].name="source_path"; params[1].type="string";   params[1].required=true;  params[1].description="Relative path for the new .diascene file.";
					params[2].name="tags";        params[2].type="string[]"; params[2].required=false; params[2].description="Tag list.";
					for (int pi = 0; pi < 3; ++pi) d.params.params.Add(params[pi]);
				}
				d.handler = [this](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					// Build create_scene request for asset_catalogue
					Json::Value createReq(Json::objectValue);
					if (data.isMember("id"))          createReq["id"]          = data["id"];
					if (data.isMember("source_path")) createReq["source_path"] = data["source_path"];
					if (data.isMember("tags"))        createReq["tags"]        = data["tags"];
					Json::Value createResult = GetBridge()->InvokeRequestHandler(
						Dia::Core::StringCRC("asset_catalogue.create_scene"), createReq);
					if (!createResult.get("success", false).asBool())
						return createResult;
					// Load the new scene
					const std::string absPath = createResult.get("absPath", "").asString();
					if (!absPath.empty())
					{
						Json::Value loadReq(Json::objectValue);
						loadReq["path"] = absPath;
						Json::Value loadResult = HandleLoadScene(loadReq);
						if (!loadResult.get("success", false).asBool())
							return loadResult;
					}
					return createResult;
				};
				api->RegisterAction(d);
			}

			// create_asset (new action — custom handler)
			{
				Dia::Editor::EditorActionDescriptor d;
				d.name           = Dia::Core::StringCRC("scene_editor.create_asset");
				d.description    = "Creates a new asset record and stub source file of any type without opening any dialog. "
				                   "Does not auto-load the asset. Use asset_catalogue.open_in_editor to open it.";
				d.category       = "scene_editor";
				d.owner          = "DiaSceneEditorPlugin";
				d.dispatchThread = DT::kMainThread;
				{
					Dia::Editor::EditorActionParam params[4];
					params[0].name="assetType";   params[0].type="string";   params[0].required=true;  params[0].description="Asset type ID.";
					params[1].name="id";          params[1].type="string";   params[1].required=true;  params[1].description="Unique asset ID.";
					params[2].name="source_path"; params[2].type="string";   params[2].required=true;  params[2].description="Relative path for the new source file.";
					params[3].name="tags";        params[3].type="string[]"; params[3].required=false; params[3].description="Tag list.";
					for (int pi = 0; pi < 4; ++pi) d.params.params.Add(params[pi]);
				}
				d.handler = [this](const Json::Value& data) -> Json::Value {
					if (!GetBridge()) { Json::Value e; e["success"]=false; e["error"]="bridge not available"; return e; }
					return GetBridge()->InvokeRequestHandler(
						Dia::Core::StringCRC("asset_catalogue.create_asset"), data);
				};
				api->RegisterAction(d);
			}
		}

		// ═══════════════════════════════════════════════════════════════════════
		// Extracted handler methods — testable without bridge/registration
		// ═══════════════════════════════════════════════════════════════════════

		Json::Value DiaSceneEditorPlugin::HandleAddItem(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.add_item", Dia::Observation::Trace::Category::kNone);
			if (!data.isMember("itemType") || !data.isMember("entityTemplateId"))
				return MakeErrorResponse("missing itemType or entityTemplateId");
			if (mLoadedSceneRoot.isNull())
				return MakeErrorResponse("no scene loaded");

			const char* userItemId = (data.isMember("id") && data["id"].isString())
				? data["id"].asCString() : nullptr;

			char err[256] = {};
			if (!SceneMutator::AddItem(mLoadedSceneRoot,
			        data["itemType"].asCString(),
			        data["entityTemplateId"].asCString(),
			        err, sizeof(err),
			        userItemId))
				return MakeErrorResponse(err);

			if (GetBridge() && mSceneCatalogueId[0] != '\0')
			{
				Json::Value relReq;
				relReq["from"] = mSceneCatalogueId;
				relReq["rel"]  = "uses";
				relReq["to"]   = data["entityTemplateId"].asString();
				GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.add_relationship"), relReq);
			}

			AutoSave();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
			return result;
		}

		Json::Value DiaSceneEditorPlugin::HandleDeleteItem(const Json::Value& data)
		{
			DIA_TRACE_ZONE("scene_editor.delete_item", Dia::Observation::Trace::Category::kNone);
			if (!data.isMember("itemType") || !data.isMember("itemId"))
				return MakeErrorResponse("missing itemType or itemId");
			if (mLoadedSceneRoot.isNull())
				return MakeErrorResponse("no scene loaded");

			char entityTemplateIdForRemove[256] = {};
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
							const char* etBuf = bp.isString() ? bp.asCString()
							                  : (bp.isObject() && bp.isMember("value") ? bp["value"].asCString() : "");
							strncpy_s(entityTemplateIdForRemove, sizeof(entityTemplateIdForRemove), etBuf, _TRUNCATE);
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

			if (GetBridge() && mSceneCatalogueId[0] != '\0' && entityTemplateIdForRemove[0] != '\0')
			{
				Json::Value relReq;
				relReq["from"] = mSceneCatalogueId;
				relReq["rel"]  = "uses";
				relReq["to"]   = entityTemplateIdForRemove;
				GetBridge()->InvokeRequestHandler(Dia::Core::StringCRC("asset_catalogue.remove_relationship"), relReq);
			}

			mHierarchyController.ClearSelection();
			AutoSave();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
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

			AutoSave();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
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
			AutoSave();
			Json::Value result;
			result["success"]   = true;
			result["hierarchy"] = BuildEnrichedHierarchy(mLoadedSceneRoot);
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
			result["hierarchy"] = BuildEnrichedHierarchy(sceneRoot);
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

		void DiaSceneEditorPlugin::AutoSave()
		{
			if (mLoadedScenePath[0] == '\0' || mLoadedSceneRoot.isNull())
				return;

			char err[256] = {};
			if (mFileHandler.Save(mLoadedScenePath, mLoadedSceneRoot, err, sizeof(err)))
			{
				ClearDirty();
			}
			else
			{
				DIA_LOG_ERROR("Editor", "DiaSceneEditorPlugin: autosave failed for '%s': %s",
					mLoadedScenePath, err[0] ? err : "unknown error");
			}
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
