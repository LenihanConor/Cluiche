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
		DiaBlueprintEditorPlugin::DiaBlueprintEditorPlugin()
			: EditorPluginBase({
				"DiaBlueprintEditor",
				"1.0.0",
				"Author entity, camera, and light blueprint files",
				"dia://plugins/blueprinteditor/index.html",
				Dia::Editor::LayoutMode::kDockable,
				nullptr,
				nullptr,
				false,
				true
			})
		{
		}

		void DiaBlueprintEditorPlugin::OnProjectChanged(const Dia::Editor::ProjectContext& ctx)
		{
			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnProjectChanged — IsValid=%d diagamePath='%s'",
				ctx.IsValid() ? 1 : 0, ctx.diagamePath);

			strncpy_s(mDiagamePath, kDiagamePathLength,
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

				const int prevMajor = mSchemaReader.GetVersion().major;
				mSchemaReader.LoadFromFile(schemaPath);

				if (mSchemaReader.IsLoaded())
				{
					const int newMajor = mSchemaReader.GetVersion().major;
					if (prevMajor != 0 && newMajor != prevMajor)
					{
						DIA_LOG_WARNING("Editor",
							"Blueprint: schema major version changed — blueprint files may reference stale types");
					}
				}
			}
			else
			{
				mSchemaReader.Clear();
			}

			if (GetBridge())
			{
				Json::Value payload;
				payload["diagamePath"] = ctx.diagamePath;
				payload["isValid"]     = ctx.IsValid();
				GetBridge()->NotifyUIDataChanged("blueprint_editor.project_changed", payload);
			}
		}

		void DiaBlueprintEditorPlugin::OnPluginLoad()
		{
			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnPluginLoad");

			if (GetModel() != nullptr)
			{
				const Dia::Editor::ProjectContext& proj = GetModel()->GetDiagameProject();
				strncpy_s(mDiagamePath, kDiagamePathLength,
				          proj.IsValid() ? proj.diagamePath : "", _TRUNCATE);
			}
			else
				DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: OnPluginLoad — model is null");

			RegisterRequestHandlers();

			// T2: register this plugin as the handler for blueprint asset types in the catalogue.
			RegisterAssetTypesWithCatalogue();

			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnPluginLoad complete");
		}

		void DiaBlueprintEditorPlugin::OnPluginUnload()
		{
			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnPluginUnload");
		}

		void DiaBlueprintEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
		}

		void DiaBlueprintEditorPlugin::OnNavigate(const Dia::Core::StringCRC& instanceId)
		{
		    DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin::OnNavigate: instanceId='%s'", instanceId.AsChar());

		    if (!GetBridge())
		        return;

		    // Look up the source path via the catalogue's get_record handler
		    Json::Value req;
		    req["id"] = instanceId.AsChar();
		    Json::Value rec = GetBridge()->InvokeRequestHandler(
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
		    Json::Value loadResult = GetBridge()->InvokeRequestHandler(
		        Dia::Core::StringCRC("blueprint_editor.load"), loadReq);

		    if (loadResult.isNull() || !loadResult.get("success", false).asBool())
		    {
		        DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin::OnNavigate: load failed for path '%s'", sourcePath.c_str());

		        Json::Value failData;
		        failData["instanceId"]  = instanceId.AsChar();
		        failData["sourcePath"]  = sourcePath;
		        failData["error"]       = loadResult.isNull() ? "load returned null" : loadResult.get("error", "unknown").asString();
		        failData["assetType"]   = rec["record"].get("type", "diaentity").asString();
		        GetBridge()->NotifyUIDataChanged("blueprint_editor.navigate_failed", failData);
		        return;
		    }

		    // Tell the UI to select and display this blueprint
		    Json::Value navData;
		    navData["instanceId"] = instanceId.AsChar();
		    navData["sourcePath"] = sourcePath;
		    GetBridge()->NotifyUIDataChanged("blueprint_editor.navigated", navData);

		    DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin::OnNavigate: loaded blueprint '%s'", sourcePath.c_str());
		}

		// T2 ──────────────────────────────────────────────────────────────────────────────
		void DiaBlueprintEditorPlugin::RegisterAssetTypesWithCatalogue()
		{
			if (!GetBridge())
				return;

			static const char* kTypes[] = { "diaentity", "diacamera", "dialight" };
			static const unsigned int kTypeCount = 3;

			for (unsigned int i = 0; i < kTypeCount; ++i)
			{
				Json::Value req;
				req["assetType"]       = kTypes[i];
				req["editorPluginType"] = "DiaBlueprintEditor";

				Json::Value res = GetBridge()->InvokeRequestHandler(
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
			if (!GetBridge())
				return Json::Value(Json::arrayValue);

			Json::Value req;
			req["typeId"] = typeId;
			Json::Value res = GetBridge()->InvokeRequestHandler(
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
			if (!GetBridge())
				return;

			RegisterListHandlers();
			RegisterPropertyHandlers();
			RegisterFileHandlers();
		}

		void DiaBlueprintEditorPlugin::RegisterListHandlers()
		{
			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.get_project_state"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value r;
					r["isValid"]     = mDiagamePath[0] != '\0';
					r["diagamePath"] = mDiagamePath;
					return r;
				});

			RegisterHandler(
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
			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.get_available_components"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.get_available_components", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: get_available_components — missing path");
						return MakeErrorResponse("missing path");
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: get_available_components — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
					result["success"]    = true;
					result["components"] = mPropertyController.BuildAvailableComponentsJson(blueprintRoot, topKey, mSchemaReader);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.get_usage"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.get_usage", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("assetId") || !data["assetId"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: get_usage — missing assetId");
						return MakeErrorResponse("missing assetId");
					}

					Json::Value refsReq;
					refsReq["id"] = data["assetId"].asString();
					Json::Value refsRes = GetBridge()->InvokeRequestHandler(
						Dia::Core::StringCRC("asset_catalogue.get_reverse_refs"), refsReq);

					Json::Value emptyRefs(Json::arrayValue);
					const Json::Value& refs = (refsRes.isNull() || !refsRes.get("success", false).asBool())
					                        ? emptyRefs
					                        : refsRes["refs"];

					Json::Value result;
					result["success"] = true;
					result["usage"]   = mPropertyController.BuildUsageJson(refs);
					return result;
				});
		}

		void DiaBlueprintEditorPlugin::RegisterFileHandlers()
		{
			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.load"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.load", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: load — missing path");
						return MakeErrorResponse("missing path");
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: load — failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
					Json::Value result;
					result["success"]    = true;
					result["properties"] = mPropertyController.BuildPropertyJson(blueprintRoot, topKey, mSchemaReader);
					DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: loaded blueprint '%s'",
						data["path"].asCString());
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.save"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.save", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("blueprint"))
					{
						DIA_LOG_WARNING("Editor", "DiaBlueprintEditorPlugin: save — missing path or blueprint");
						return MakeErrorResponse("missing path or blueprint");
					}

					char err[256] = {};
					if (!mFileHandler.Save(data["path"].asCString(), data["blueprint"], err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: save — failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: saved blueprint '%s'",
						data["path"].asCString());
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.update_field"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.update_field", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data.isMember("fieldName")
					    || !data.isMember("value"))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — missing required fields");
						return MakeErrorResponse("missing path, componentType, fieldName, or value");
					}

					// Handle null value: remove the field from the component
					if (data["value"].isNull())
					{
						Json::Value blueprintRoot;
						char err[256] = {};
						if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
						{
							DIA_LOG_WARNING("Editor",
								"DiaBlueprintEditorPlugin: update_field — load failed for '%s': %s",
								data["path"].asCString(), err);
							return MakeErrorResponse(err[0] ? err : "load failed");
						}

						const char* ext    = strrchr(data["path"].asCString(), '.');
						const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

						if (!BlueprintMutator::ClearField(blueprintRoot, topKey,
								data["componentType"].asCString(),
								data["fieldName"].asCString(),
								err, sizeof(err)))
						{
							DIA_LOG_WARNING("Editor",
								"DiaBlueprintEditorPlugin: update_field — clear field failed for '%s': %s",
								data["path"].asCString(), err);
							return MakeErrorResponse(err[0] ? err : "clear field failed");
						}

						if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
						{
							DIA_LOG_WARNING("Editor",
								"DiaBlueprintEditorPlugin: update_field — save failed for '%s': %s",
								data["path"].asCString(), err);
							return MakeErrorResponse(err[0] ? err : "save failed");
						}

						return MakeSuccessResponse();
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
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
						return MakeErrorResponse(err[0] ? err : "mutation failed");
					}

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — save failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.add_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.add_component", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — missing path or componentType");
						return MakeErrorResponse("missing path or componentType");
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::AddComponent(blueprintRoot, topKey,
					        data["componentType"].asCString(), err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "mutation failed");
					}

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — save failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor",
						"DiaBlueprintEditorPlugin: added component '%s' to '%s'",
						data["componentType"].asCString(), data["path"].asCString());
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.remove_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.remove_component", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — missing path or componentType");
						return MakeErrorResponse("missing path or componentType");
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::RemoveComponent(blueprintRoot, topKey,
					        data["componentType"].asCString(), err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "mutation failed");
					}

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — save failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor",
						"DiaBlueprintEditorPlugin: removed component '%s' from '%s'",
						data["componentType"].asCString(), data["path"].asCString());
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("blueprint_editor.create_from_template"),
				[this](const Json::Value& data) -> Json::Value
				{
					if (!data.isMember("instanceId") || !data["instanceId"].isString()
					    || !data.isMember("path") || !data["path"].isString())
					{
						return MakeErrorResponse("missing instanceId or path");
					}

					const std::string instanceId = data["instanceId"].asString();
					const std::string path       = data["path"].asString();

					const char* ext    = strrchr(path.c_str(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					Json::Value blueprintRoot;
					Json::Value& inner = blueprintRoot[topKey];
					inner["id"]         = instanceId;
					inner["components"] = Json::Value(Json::arrayValue);

					char err[256] = {};
					if (!mFileHandler.Save(path.c_str(), blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: create_from_template — save failed for '%s': %s",
							path.c_str(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor",
						"DiaBlueprintEditorPlugin: created template file '%s' for '%s'",
						path.c_str(), instanceId.c_str());
					return MakeSuccessResponse();
				});
		}

	}
}
