#include "DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.h"
#include "DiaEntityTemplateEditor/BlueprintMutator.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>

using namespace Dia::EntityTemplateEditor;

REGISTER_EDITOR_PLUGIN(DiaEntityTemplateEditorPlugin, "DiaEntityTemplateEditor")

namespace Dia
{
	namespace EntityTemplateEditor
	{
		DiaEntityTemplateEditorPlugin::DiaEntityTemplateEditorPlugin()
			: EditorPluginBase({
				"DiaEntityTemplateEditor",
				"1.0.0",
				"Author entity, camera, and light blueprint files",
				"dia://plugins/EntityTemplateEditor/index.html",
				Dia::Editor::LayoutMode::kDockable,
				nullptr,
				nullptr,
				false
			})
		{
		}

		void DiaEntityTemplateEditorPlugin::ResolvePath(const char* relPath, char* absOut, unsigned int absCapacity) const
		{
			if (!relPath || relPath[0] == '\0') { if (absOut && absCapacity > 0) absOut[0] = '\0'; return; }

			// Already absolute
			bool isAbsolute = (relPath[0] == '/' || relPath[0] == '\\' || (relPath[1] == ':'));
			if (isAbsolute || mDiagameDir[0] == '\0')
			{
				strncpy_s(absOut, absCapacity, relPath, _TRUNCATE);
				return;
			}
			_snprintf_s(absOut, absCapacity, _TRUNCATE, "%s%s", mDiagameDir, relPath);
		}

		void DiaEntityTemplateEditorPlugin::OnProjectChanged(const Dia::Editor::ProjectContext& ctx)
		{
			DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: OnProjectChanged — IsValid=%d diagamePath='%s'",
				ctx.IsValid() ? 1 : 0, ctx.diagamePath);

			strncpy_s(mDiagamePath, kDiagamePathLength,
			          ctx.IsValid() ? ctx.diagamePath : "", _TRUNCATE);

			mDiagameDir[0] = '\0';
			if (ctx.IsValid() && ctx.diagamePath[0] != '\0')
			{
				strncpy_s(mDiagameDir, kDiagameDirLength, ctx.diagamePath, _TRUNCATE);
				char* lastSep = nullptr;
				for (char* p = mDiagameDir; *p; ++p)
					if (*p == '/' || *p == '\\') lastSep = p;
				if (lastSep) *(lastSep + 1) = '\0';
				else mDiagameDir[0] = '\0';
			}

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
				GetBridge()->NotifyUIDataChanged("entity_template_editor.project_changed", payload);
			}
		}

		void DiaEntityTemplateEditorPlugin::OnPluginLoad()
		{
			DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: OnPluginLoad");

			if (GetModel() != nullptr)
			{
				const Dia::Editor::ProjectContext& proj = GetModel()->GetDiagameProject();
				strncpy_s(mDiagamePath, kDiagamePathLength,
				          proj.IsValid() ? proj.diagamePath : "", _TRUNCATE);
			}
			else
				DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin: OnPluginLoad — model is null");

			RegisterRequestHandlers();

			// T2: register this plugin as the handler for blueprint asset types in the catalogue.
			RegisterAssetTypesWithCatalogue();

			DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: OnPluginLoad complete");
		}

		void DiaEntityTemplateEditorPlugin::OnPluginUnload()
		{
			DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: OnPluginUnload");
		}

		void DiaEntityTemplateEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
		}

		void DiaEntityTemplateEditorPlugin::OnNavigate(const Dia::Core::StringCRC& instanceId)
		{
		    if (instanceId == Dia::Core::StringCRC::kZero)
		        return;

		    DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin::OnNavigate: instanceId='%s'", instanceId.AsChar());

		    if (!GetBridge())
		        return;

		    // Look up the source path via the catalogue's get_record handler
		    Json::Value req;
		    req["id"] = instanceId.AsChar();
		    Json::Value rec = GetBridge()->InvokeRequestHandler(
		        Dia::Core::StringCRC("asset_catalogue.get_record"), req);

		    if (rec.isNull() || !rec.get("success", false).asBool())
		    {
		        DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin::OnNavigate: get_record failed for '%s' — catalogue may not be loaded", instanceId.AsChar());
		        return;
		    }

		    const std::string sourcePath = rec["record"].get("source_path", "").asString();
		    if (sourcePath.empty())
		    {
		        DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin::OnNavigate: no source_path for '%s'", instanceId.AsChar());
		        return;
		    }

		    // Load the blueprint using the existing handler
		    Json::Value loadReq;
		    loadReq["path"] = sourcePath;
		    Json::Value loadResult = GetBridge()->InvokeRequestHandler(
		        Dia::Core::StringCRC("entity_template_editor.load"), loadReq);

		    if (loadResult.isNull() || !loadResult.get("success", false).asBool())
		    {
		        DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin::OnNavigate: load failed for path '%s'", sourcePath.c_str());

		        Json::Value failData;
		        failData["instanceId"]  = instanceId.AsChar();
		        failData["sourcePath"]  = sourcePath;
		        failData["error"]       = loadResult.isNull() ? "load returned null" : loadResult.get("error", "unknown").asString();
		        failData["assetType"]   = rec["record"].get("type", "diaentitytemplate").asString();
		        GetBridge()->NotifyUIDataChanged("entity_template_editor.navigate_failed", failData);
		        return;
		    }

		    // Tell the UI to select and display this blueprint
		    Json::Value navData;
		    navData["instanceId"] = instanceId.AsChar();
		    navData["sourcePath"] = sourcePath;
		    GetBridge()->NotifyUIDataChanged("entity_template_editor.navigated", navData);

		    DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin::OnNavigate: loaded blueprint '%s'", sourcePath.c_str());
		}

		// T2 ──────────────────────────────────────────────────────────────────────────────
		void DiaEntityTemplateEditorPlugin::RegisterAssetTypesWithCatalogue()
		{
			if (!GetBridge())
				return;

			static const char* kTypes[] = { "diaentitytemplate", "diacamera", "dialight" };
			static const unsigned int kTypeCount = 3;

			for (unsigned int i = 0; i < kTypeCount; ++i)
			{
				Json::Value req;
				req["assetType"]       = kTypes[i];
				req["editorPluginType"] = "DiaEntityTemplateEditor";

				Json::Value res = GetBridge()->InvokeRequestHandler(
					Dia::Core::StringCRC("asset_catalogue.register_type_editor"), req);

				if (!res.isNull() && res.isMember("success") && !res["success"].asBool())
				{
					DIA_LOG_WARNING("Editor",
						"DiaEntityTemplateEditorPlugin: failed to register type editor for '%s': %s",
						kTypes[i], res.get("error", "unknown").asCString());
				}
				else
				{
					DIA_LOG_INFO("Editor",
						"DiaEntityTemplateEditorPlugin: registered type editor for '%s'", kTypes[i]);
				}
			}
		}

		// ─────────────────────────────────────────────────────────────────────────────────
		Json::Value DiaEntityTemplateEditorPlugin::QueryCatalogueByType(const char* typeId) const
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
					"DiaEntityTemplateEditorPlugin: query_by_type failed for typeId='%s'", typeId);
				return Json::Value(Json::arrayValue);
			}
			return res["records"];
		}

		void DiaEntityTemplateEditorPlugin::RegisterRequestHandlers()
		{
			if (!GetBridge())
				return;

			RegisterListHandlers();
			RegisterPropertyHandlers();
			RegisterFileHandlers();
		}

		void DiaEntityTemplateEditorPlugin::RegisterListHandlers()
		{
			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.get_project_state"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value r;
					r["isValid"]     = mDiagamePath[0] != '\0';
					r["diagamePath"] = mDiagamePath;
					return r;
				});

			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.get_list"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.get_list", Dia::Observation::Trace::Category::kNone);
					return mListController.BuildListJson(
						QueryCatalogueByType("diaentitytemplate"),
						QueryCatalogueByType("diacamera"),
						QueryCatalogueByType("dialight"));
				});
		}

		void DiaEntityTemplateEditorPlugin::RegisterPropertyHandlers()
		{
			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.get_available_components"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.get_available_components", Dia::Observation::Trace::Category::kNone);
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin: get_available_components — missing path");
						return MakeErrorResponse("missing path");
					}

					char resolvedPath[512] = {};
					ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: get_available_components — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(resolvedPath, '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
					result["success"]    = true;
					result["components"] = mPropertyController.BuildAvailableComponentsJson(blueprintRoot, topKey, mSchemaReader);
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.get_usage"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.get_usage", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("assetId") || !data["assetId"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin: get_usage — missing assetId");
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

		void DiaEntityTemplateEditorPlugin::RegisterFileHandlers()
		{
			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.load"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.load", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString())
					{
						DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin: load — missing path");
						return MakeErrorResponse("missing path");
					}

					char resolvedPath[512] = {};
					ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: load — failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(resolvedPath, '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
					Json::Value result;
					result["success"]    = true;
					result["properties"] = mPropertyController.BuildPropertyJson(blueprintRoot, topKey, mSchemaReader);
					DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: loaded blueprint '%s'",
						data["path"].asCString());
					return result;
				});

			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.save"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.save", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("blueprint"))
					{
						DIA_LOG_WARNING("Editor", "DiaEntityTemplateEditorPlugin: save — missing path or blueprint");
						return MakeErrorResponse("missing path or blueprint");
					}

					char resolvedPath[512] = {};
					ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
					char err[256] = {};
					if (!mFileHandler.Save(resolvedPath, data["blueprint"], err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: save — failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: saved blueprint '%s'",
						data["path"].asCString());
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.update_field"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.update_field", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data.isMember("fieldName")
					    || !data.isMember("value"))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: update_field — missing required fields");
						return MakeErrorResponse("missing path, componentType, fieldName, or value");
					}

					// Handle null value: remove the field from the component
					if (data["value"].isNull())
					{
						char resolvedPath[512] = {};
						ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
						Json::Value blueprintRoot;
						char err[256] = {};
						if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
						{
							DIA_LOG_WARNING("Editor",
								"DiaEntityTemplateEditorPlugin: update_field — load failed for '%s': %s",
								data["path"].asCString(), err);
							return MakeErrorResponse(err[0] ? err : "load failed");
						}

						const char* ext    = strrchr(resolvedPath, '.');
						const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

						if (!BlueprintMutator::ClearField(blueprintRoot, topKey,
								data["componentType"].asCString(),
								data["fieldName"].asCString(),
								err, sizeof(err)))
						{
							DIA_LOG_WARNING("Editor",
								"DiaEntityTemplateEditorPlugin: update_field — clear field failed for '%s': %s",
								data["path"].asCString(), err);
							return MakeErrorResponse(err[0] ? err : "clear field failed");
						}

						if (!mFileHandler.Save(resolvedPath, blueprintRoot, err, sizeof(err)))
						{
							DIA_LOG_WARNING("Editor",
								"DiaEntityTemplateEditorPlugin: update_field — save failed for '%s': %s",
								data["path"].asCString(), err);
							return MakeErrorResponse(err[0] ? err : "save failed");
						}

						return MakeSuccessResponse();
					}

					char resolvedPath[512] = {};
					ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: update_field — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(resolvedPath, '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::UpdateField(blueprintRoot, topKey,
					        data["componentType"].asCString(),
					        data["fieldName"].asCString(),
					        data["value"], err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: update_field — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "mutation failed");
					}

					if (!mFileHandler.Save(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: update_field — save failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.add_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.add_component", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: add_component — missing path or componentType");
						return MakeErrorResponse("missing path or componentType");
					}

					char resolvedPath[512] = {};
					ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: add_component — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(resolvedPath, '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::AddComponent(blueprintRoot, topKey,
					        data["componentType"].asCString(), err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: add_component — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "mutation failed");
					}

					if (!mFileHandler.Save(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: add_component — save failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor",
						"DiaEntityTemplateEditorPlugin: added component '%s' to '%s'",
						data["componentType"].asCString(), data["path"].asCString());
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.remove_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					DIA_TRACE_ZONE("entity_template_editor.remove_component", Dia::Observation::Trace::Category::kNone);
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: remove_component — missing path or componentType");
						return MakeErrorResponse("missing path or componentType");
					}

					char resolvedPath[512] = {};
					ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: remove_component — load failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "load failed");
					}

					const char* ext    = strrchr(resolvedPath, '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!BlueprintMutator::RemoveComponent(blueprintRoot, topKey,
					        data["componentType"].asCString(), err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: remove_component — mutation failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "mutation failed");
					}

					if (!mFileHandler.Save(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: remove_component — save failed for '%s': %s",
							data["path"].asCString(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor",
						"DiaEntityTemplateEditorPlugin: removed component '%s' from '%s'",
						data["componentType"].asCString(), data["path"].asCString());
					return MakeSuccessResponse();
				});

			RegisterHandler(
				Dia::Core::StringCRC("entity_template_editor.create_from_template"),
				[this](const Json::Value& data) -> Json::Value
				{
					if (!data.isMember("instanceId") || !data["instanceId"].isString()
					    || !data.isMember("path") || !data["path"].isString())
					{
						return MakeErrorResponse("missing instanceId or path");
					}

					const std::string instanceId = data["instanceId"].asString();
					const std::string path = data["path"].asString();
					char resolvedPath[512] = {};
					ResolvePath(path.c_str(), resolvedPath, sizeof(resolvedPath));

					const char* ext    = strrchr(resolvedPath, '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					Json::Value blueprintRoot;
					Json::Value& inner = blueprintRoot[topKey];
					inner["id"]         = instanceId;
					inner["components"] = Json::Value(Json::arrayValue);

					char err[256] = {};
					if (!mFileHandler.Save(resolvedPath, blueprintRoot, err, sizeof(err)))
					{
						DIA_LOG_WARNING("Editor",
							"DiaEntityTemplateEditorPlugin: create_from_template — save failed for '%s': %s",
							path.c_str(), err);
						return MakeErrorResponse(err[0] ? err : "save failed");
					}

					DIA_LOG_INFO("Editor",
						"DiaEntityTemplateEditorPlugin: created template file '%s' for '%s'",
						path.c_str(), instanceId.c_str());
					return MakeSuccessResponse();
				});

		}

	}
}
