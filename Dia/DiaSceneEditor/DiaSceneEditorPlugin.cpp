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

			if (self->mBridge)
				self->mBridge->NotifyUIDataChanged("scene_editor.project_changed", Json::Value(ctx.diagamePath));
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
