#include "DiaBlueprintEditor/DiaBlueprintEditorPlugin.h"
#include "DiaBlueprintEditor/BlueprintMutator.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>

using namespace Dia::BlueprintEditor;

REGISTER_EDITOR_PLUGIN(DiaBlueprintEditorPlugin, "DiaBlueprintEditor")

namespace Dia
{
	namespace BlueprintEditor
	{
		void DiaBlueprintEditorPlugin::OnProjectChangedStatic(const Dia::Editor::ProjectContext& ctx, void* ud)
		{
			auto* self = static_cast<DiaBlueprintEditorPlugin*>(ud);
			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnProjectChanged — IsValid=%d diagamePath='%s'",
				ctx.IsValid() ? 1 : 0, ctx.diagamePath);

			strncpy_s(self->mDiagamePath, self->kDiagamePathLength,
			          ctx.IsValid() ? ctx.diagamePath : "", _TRUNCATE);

			// Derive schema path: replace the .diagame filename with registeredtypes.diaschema
			if (ctx.IsValid() && ctx.diagamePath[0] != '\0')
			{
				char schemaPath[512] = {};
				strncpy_s(schemaPath, sizeof(schemaPath), ctx.diagamePath, _TRUNCATE);

				// Find last slash or backslash
				char* lastSep = nullptr;
				for (char* p = schemaPath; *p; ++p)
				{
					if (*p == '/' || *p == '\\')
						lastSep = p;
				}

				if (lastSep)
					*(lastSep + 1) = '\0';
				else
					schemaPath[0] = '\0'; // no directory part — use current dir

				strncat_s(schemaPath, sizeof(schemaPath), "registeredtypes.diaschema", _TRUNCATE);

				const int prevMajor = self->mSchemaReader.GetVersion().major;
				self->mSchemaReader.LoadFromFile(schemaPath);

				if (self->mSchemaReader.IsLoaded())
				{
					const int newMajor = self->mSchemaReader.GetVersion().major;
					if (prevMajor != 0 && newMajor != prevMajor)
					{
						DIA_LOG_WARNING("Editor",
							"Blueprint: schema major version changed — blueprint files may reference stale types");
					}
				}
			}
			else
			{
				self->mSchemaReader.Clear();
			}

			if (self->mBridge)
			{
				Json::Value payload;
				payload["diagamePath"] = ctx.diagamePath;
				payload["isValid"]     = ctx.IsValid();
				self->mBridge->NotifyUIDataChanged("blueprint_editor.project_changed", payload);
			}
		}

		void DiaBlueprintEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
		{
			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnLoad");

			mBridge       = context.mBridge;
			mPluginLoader = context.mPluginLoader;

			RegisterRequestHandlers();

			if (context.mModel != nullptr)
			{
				context.mModel->OnDiagameProjectChanged(&DiaBlueprintEditorPlugin::OnProjectChangedStatic, this);

				// Populate state from current project so get_project_state is correct
				// when the UI polls on init. Do not push — the UI isn't ready yet.
				const Dia::Editor::ProjectContext& proj = context.mModel->GetDiagameProject();
				strncpy_s(mDiagamePath, kDiagamePathLength,
				          proj.IsValid() ? proj.diagamePath : "", _TRUNCATE);
			}
			else
				DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: OnLoad — context.mModel is null");

			// T2: register this plugin as the handler for blueprint asset types in the catalogue.
			RegisterAssetTypesWithCatalogue();

			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnLoad complete");
		}

		void DiaBlueprintEditorPlugin::OnUnload()
		{
			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnUnload");

			if (mBridge)
			{
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.get_project_state"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.get_list"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.load"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.save"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.get_available_components"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.add_component"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.remove_component"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.update_field"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.get_usage"));
			}

			mBridge       = nullptr;
			mPluginLoader = nullptr;
		}

		void DiaBlueprintEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
		}

		void DiaBlueprintEditorPlugin::OnNavigate(const Dia::Core::StringCRC& instanceId)
		{
		    DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin::OnNavigate: instanceId='%s'", instanceId.AsChar());

		    if (!mBridge)
		        return;

		    // Look up the source path via the catalogue's get_record handler
		    Json::Value req;
		    req["id"] = instanceId.AsChar();
		    Json::Value rec = mBridge->InvokeRequestHandler(
		        Dia::Core::StringCRC("asset_catalogue.get_record"), req);

		    if (rec.isNull() || !rec.get("success", false).asBool())
		    {
		        DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin::OnNavigate: get_record failed for '%s' — catalogue may not be loaded", instanceId.AsChar());
		        return;
		    }

		    const std::string sourcePath = rec["record"].get("source_path", "").asString();
		    if (sourcePath.empty())
		    {
		        DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin::OnNavigate: no source_path for '%s'", instanceId.AsChar());
		        return;
		    }

		    // Load the blueprint using the existing handler
		    Json::Value loadReq;
		    loadReq["path"] = sourcePath;
		    Json::Value loadResult = mBridge->InvokeRequestHandler(
		        Dia::Core::StringCRC("blueprint_editor.load"), loadReq);

		    if (loadResult.isNull() || !loadResult.get("success", false).asBool())
		    {
		        DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin::OnNavigate: load failed for path '%s'", sourcePath.c_str());
		        return;
		    }

		    DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin::OnNavigate: loaded blueprint '%s'", sourcePath.c_str());
		}

		// T2 ──────────────────────────────────────────────────────────────────────────────
		void DiaBlueprintEditorPlugin::RegisterAssetTypesWithCatalogue()
		{
			if (!mBridge)
				return;

			static const char* kTypes[] = { "diaentity", "diacamera", "dialight" };
			static const unsigned int kTypeCount = 3;

			for (unsigned int i = 0; i < kTypeCount; ++i)
			{
				Json::Value req;
				req["assetType"]       = kTypes[i];
				req["editorPluginType"] = "DiaBlueprintEditor";

				Json::Value res = mBridge->InvokeRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.register_type_editor"), req);

				if (!res.isNull() && res.isMember("success") && !res["success"].asBool())
				{
					DIA_LOG_WARNING("Editor",
						"DiaBlueprintEditorPlugin: failed to register type editor for '%s': %s",
						kTypes[i], res.get("error", "unknown").asCString());
				}
				else
				{
					DIA_LOG_INFO("Editor",
						"DiaBlueprintEditorPlugin: registered type editor for '%s'", kTypes[i]);
				}
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────────
		Json::Value DiaBlueprintEditorPlugin::QueryCatalogueByType(const char* typeId) const
		{
			if (!mBridge)
				return Json::Value(Json::arrayValue);

			Json::Value req;
			req["typeId"] = typeId;
			Json::Value res = mBridge->InvokeRequestHandler(
				Dia::Core::StringCRC("asset_catalogue.query_by_type"), req);

			if (res.isNull() || !res.get("success", false).asBool())
			{
				DIA_LOG_WARNING("Editor",
					"DiaBlueprintEditorPlugin: query_by_type failed for typeId='%s'", typeId);
				return Json::Value(Json::arrayValue);
			}
			return res["records"];
		}

		void DiaBlueprintEditorPlugin::RegisterRequestHandlers()
		{
			if (!mBridge)
				return;

			RegisterListHandlers();
			RegisterPropertyHandlers();
			RegisterFileHandlers();
		}

		void DiaBlueprintEditorPlugin::RegisterListHandlers()
		{
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_project_state"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value r;
					r["isValid"]     = mDiagamePath[0] != '\0';
					r["diagamePath"] = mDiagamePath;
					return r;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_list"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.get_list", Dia::Observation::Trace::Category::kNone);
					return mListController.BuildListJson(
						QueryCatalogueByType("diaentity"),
						QueryCatalogueByType("diacamera"),
						QueryCatalogueByType("dialight"));
				});
		}

		void DiaBlueprintEditorPlugin::RegisterPropertyHandlers()
		{
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_available_components"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.get_available_components", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: get_available_components — missing path");
						result["success"] = false;
						result["error"]   = "missing path";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: get_available_components — load failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
					result["success"]    = true;
					result["components"] = mPropertyController.BuildAvailableComponentsJson(blueprintRoot, topKey, mSchemaReader);
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_usage"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.get_usage", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("assetId") || !data["assetId"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: get_usage — missing assetId");
						result["success"] = false;
						result["error"]   = "missing assetId";
						return result;
					}

					Json::Value refsReq;
					refsReq["id"] = data["assetId"].asString();
					Json::Value refsRes = mBridge->InvokeRequestHandler(
						Dia::Core::StringCRC("asset_catalogue.get_reverse_refs"), refsReq);

					Json::Value emptyRefs(Json::arrayValue);
					const Json::Value& refs = (refsRes.isNull() || !refsRes.get("success", false).asBool())
					                        ? emptyRefs
					                        : refsRes["refs"];

					result["success"] = true;
					result["usage"]   = mPropertyController.BuildUsageJson(refs);
					return result;
				});
		}

		void DiaBlueprintEditorPlugin::RegisterFileHandlers()
		{
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.load"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.load", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: load — missing path");
						result["success"] = false;
						result["error"]   = "missing path";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: load — failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
					result["success"]    = true;
					result["properties"] = mPropertyController.BuildPropertyJson(blueprintRoot, topKey);
					DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: loaded blueprint '%s'",
						data["path"].asCString());
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.save"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.save", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("blueprint"))
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: save — missing path or blueprint");
						result["success"] = false;
						result["error"]   = "missing path or blueprint";
						return result;
					}

					char err[256] = {};
					if (!mFileHandler.Save(data["path"].asCString(), data["blueprint"], err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: save — failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: saved blueprint '%s'",
						data["path"].asCString());
					result["success"] = true;
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.update_field"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.update_field", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data.isMember("fieldName")
					    || !data.isMember("value"))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — missing required fields");
						result["success"] = false;
						result["error"]   = "missing path, componentType, fieldName, or value";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — load failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::UpdateField(blueprintRoot, topKey,
					        data["componentType"].asCString(),
					        data["fieldName"].asCString(),
					        data["value"], err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "mutation failed";
						return result;
					}

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — save failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					result["success"] = true;
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.add_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.add_component", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — missing path or componentType");
						result["success"] = false;
						result["error"]   = "missing path or componentType";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — load failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::AddComponent(blueprintRoot, topKey,
					        data["componentType"].asCString(), err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "mutation failed";
						return result;
					}

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — save failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					DIA_LOG_INFO("Editor",
						"DiaBlueprintEditorPlugin: added component '%s' to '%s'",
						data["componentType"].asCString(), data["path"].asCString());
					result["success"] = true;
					return result;
				});

			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.remove_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.remove_component", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — missing path or componentType");
						result["success"] = false;
						result["error"]   = "missing path or componentType";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — load failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::RemoveComponent(blueprintRoot, topKey,
					        data["componentType"].asCString(), err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "mutation failed";
						return result;
					}

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — save failed for '%s': %s",
							data["path"].asCString(), err);
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					DIA_LOG_INFO("Editor",
						"DiaBlueprintEditorPlugin: removed component '%s' from '%s'",
						data["componentType"].asCString(), data["path"].asCString());
					result["success"] = true;
					return result;
				});
		}

	}
}
