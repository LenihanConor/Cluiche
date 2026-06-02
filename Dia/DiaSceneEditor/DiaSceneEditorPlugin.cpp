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

			mBridge       = context.mBridge;
			mPluginLoader = context.mPluginLoader;

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
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.get_properties"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.load_scene"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("scene_editor.save_scene"));
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
					result["success"]   = true;
					result["stage"]     = matchedStage;
					result["scene"]     = sceneRoot;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
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
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("selectionType") || !data.isMember("selectionId"))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: get_properties — missing required fields");
						result["success"] = false;
						result["error"]   = "missing path, selectionType, or selectionId";
						return result;
					}

					Json::Value sceneRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), sceneRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaSceneEditorPlugin: get_properties — load failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					result["success"]    = true;
					result["properties"] = mPropertyController.BuildPropertyJson(
						sceneRoot,
						data["selectionType"].asCString(),
						data["selectionId"].asCString());
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
					result["success"]   = true;
					result["scene"]     = sceneRoot;
					result["hierarchy"] = mHierarchyController.BuildHierarchyJson(sceneRoot);
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("scene_editor.save_scene"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("scene_editor.save_scene", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("scene"))
					{
						DIA_LOG_WARNING("Editor", "DiaSceneEditorPlugin: save_scene — missing path or scene");
						result["success"] = false;
						result["error"]   = "missing path or scene";
						return result;
					}

					char err[256] = {};
					if (!mFileHandler.Save(data["path"].asCString(), data["scene"], err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaSceneEditorPlugin: save_scene — failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					DIA_LOG_INFO("Editor", "DiaSceneEditorPlugin: saved scene '%s'",
						data["path"].asCString());
					result["success"] = true;
					return result;
				});
		}
	}
}
