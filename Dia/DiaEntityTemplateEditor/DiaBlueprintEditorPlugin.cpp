#include "DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.h"
#include "DiaEntityTemplateEditor/BlueprintMutator.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/AppEditor/AppEditorController.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>
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

			DualRegisterActions();

			DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: OnPluginLoad complete");
		}

		void DiaEntityTemplateEditorPlugin::OnPluginUnload()
		{
			if (GetServices() != nullptr)
			{
				Dia::Editor::EditorActionRegistryService* regSvc =
					GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
				if (regSvc != nullptr && regSvc->GetRegistry() != nullptr)
				{
					regSvc->GetRegistry()->DeregisterActionsForOwner(
						Dia::Core::StringCRC("DiaBlueprintEditorPlugin"));
				}
			}
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

		    // Report edit target to AppEditorController.
		    if (GetServices() != nullptr)
		    {
		        Dia::Editor::AppEditorController* ctrl =
		            GetServices()->GetService<Dia::Editor::AppEditorController>();
		        if (ctrl != nullptr)
		        {
		            ctrl->SetEditTarget(
		                Dia::Core::StringCRC("entity_template"),
		                instanceId,
		                sourcePath.c_str(),
		                false);
		            ctrl->SetFocus(Dia::Core::StringCRC("DiaEntityTemplateEditorPlugin"), "entity_template_editor");
		        }
		    }
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

		}

		// ─────────────────────────────────────────────────────────────────────────────────
		void DiaEntityTemplateEditorPlugin::DualRegisterActions()
		{
			if (GetServices() == nullptr) return;
			Dia::Editor::EditorActionRegistryService* regSvc =
				GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
			if (regSvc == nullptr || regSvc->GetRegistry() == nullptr) return;
			Dia::Editor::EditorActionRegistry* api = regSvc->GetRegistry();

			// Helper lambda to register a single action descriptor
			auto reg = [&](const char* name, const char* description, const char* category,
			               Dia::Editor::ActionHandler handler)
			{
				Dia::Editor::EditorActionDescriptor d;
				d.name           = Dia::Core::StringCRC(name);
				d.description    = description;
				d.category       = category;
				d.owner          = "DiaBlueprintEditorPlugin";
				d.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
				d.handler        = std::move(handler);
				api->RegisterAction(d);
			};

			auto regP = [&](const char* name, const char* description, const char* category,
			                Dia::Editor::ActionHandler handler,
			                const Dia::Editor::EditorActionParam* paramArr, unsigned int paramCount)
			{
				Dia::Editor::EditorActionDescriptor d;
				d.name           = Dia::Core::StringCRC(name);
				d.description    = description;
				d.category       = category;
				d.owner          = "DiaBlueprintEditorPlugin";
				d.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
				d.handler        = std::move(handler);
				for (unsigned int i = 0; i < paramCount; ++i)
					d.params.params.Add(paramArr[i]);
				api->RegisterAction(d);
			};

			// 1. get_project_state
			reg("entity_template_editor.get_project_state",
			    "Returns whether a valid project is loaded and the path to the active .diagame file. "
			    "Call this before any other entity_template_editor action to confirm the plugin has "
			    "a project context. isValid is false if no .diagame has been set.",
			    "entity_template_editor",
			    [this](const Json::Value& /*data*/) -> Json::Value {
			        Json::Value r;
			        r["isValid"]     = mDiagamePath[0] != '\0';
			        r["diagamePath"] = mDiagamePath;
			        return r;
			    });

			// 2. get_list
			reg("entity_template_editor.get_list",
			    "Returns a list of all entity templates, cameras, and lights registered in the asset "
			    "catalogue for the current project. Use this to enumerate available templates before "
			    "calling load or get_available_components.",
			    "entity_template_editor",
			    [this](const Json::Value& /*data*/) -> Json::Value {
			        return mListController.BuildListJson(
			            QueryCatalogueByType("diaentitytemplate"),
			            QueryCatalogueByType("diacamera"),
			            QueryCatalogueByType("dialight"));
			    });

			// 3. get_available_components
			{
				Dia::Editor::EditorActionParam pathParam;
				pathParam.name        = "path";
				pathParam.type        = "string";
				pathParam.required    = true;
				pathParam.description = "Absolute or catalogue-relative path to the .diablueprint file.";
				regP("entity_template_editor.get_available_components",
				     "Loads the blueprint file at path and returns the list of component types available "
				     "to be added to it — components registered in the schema but not yet present in the "
				     "file. Use before calling add_component to know what can be added.",
				     "entity_template_editor",
				     [this](const Json::Value& data) -> Json::Value {
				         if (!data.isMember("path") || !data["path"].isString())
				             return MakeErrorResponse("missing path");
				         char resolvedPath[512] = {};
				         ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
				         Json::Value blueprintRoot;
				         char err[256] = {};
				         if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
				             return MakeErrorResponse(err[0] ? err : "load failed");
				         const char* ext    = strrchr(resolvedPath, '.');
				         const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
				         Json::Value result;
				         result["success"]    = true;
				         result["components"] = mPropertyController.BuildAvailableComponentsJson(blueprintRoot, topKey, mSchemaReader);
				         return result;
				     },
				     &pathParam, 1);
			}

			// 4. get_usage
			{
				Dia::Editor::EditorActionParam assetIdParam;
				assetIdParam.name        = "assetId";
				assetIdParam.type        = "string";
				assetIdParam.required    = true;
				assetIdParam.description = "The asset ID to check usage for.";
				regP("entity_template_editor.get_usage",
				     "Returns a list of all assets that reference the given asset ID — the reverse "
				     "relationship edges from the asset catalogue. Use before deleting a template to "
				     "find what scenes or other templates depend on it.",
				     "entity_template_editor",
				     [this](const Json::Value& data) -> Json::Value {
				         if (!data.isMember("assetId") || !data["assetId"].isString())
				             return MakeErrorResponse("missing assetId");
				         Json::Value refsReq;
				         refsReq["id"] = data["assetId"].asString();
				         Json::Value refsRes = GetBridge()->InvokeRequestHandler(
				             Dia::Core::StringCRC("asset_catalogue.get_reverse_refs"), refsReq);
				         Json::Value emptyRefs(Json::arrayValue);
				         const Json::Value& refs = (refsRes.isNull() || !refsRes.get("success", false).asBool())
				                                 ? emptyRefs : refsRes["refs"];
				         Json::Value result;
				         result["success"] = true;
				         result["usage"]   = mPropertyController.BuildUsageJson(refs);
				         return result;
				     },
				     &assetIdParam, 1);
			}

			// 5. load
			{
				Dia::Editor::EditorActionParam pathParam;
				pathParam.name        = "path";
				pathParam.type        = "string";
				pathParam.required    = true;
				pathParam.description = "Absolute or catalogue-relative path to the .diablueprint file.";
				regP("entity_template_editor.load",
				     "Loads a blueprint file from disk and returns its full property structure: component "
				     "list and field values. This is a read-only operation — it does not open the file "
				     "in the editor UI. Use get_available_components afterwards to see what can still be added.",
				     "entity_template_editor",
				     [this](const Json::Value& data) -> Json::Value {
				         if (!data.isMember("path") || !data["path"].isString())
				             return MakeErrorResponse("missing path");
				         char resolvedPath[512] = {};
				         ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
				         Json::Value blueprintRoot;
				         char err[256] = {};
				         if (!mFileHandler.Load(resolvedPath, blueprintRoot, err, sizeof(err)))
				             return MakeErrorResponse(err[0] ? err : "load failed");
				         const char* ext    = strrchr(resolvedPath, '.');
				         const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
				         Json::Value result;
				         result["success"]    = true;
				         result["properties"] = mPropertyController.BuildPropertyJson(blueprintRoot, topKey, mSchemaReader);
				         return result;
				     },
				     &pathParam, 1);
			}

			// 6. save
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name = "path";      params[0].type = "string"; params[0].required = true; params[0].description = "Absolute path to write the .diablueprint file.";
				params[1].name = "blueprint"; params[1].type = "object"; params[1].required = true; params[1].description = "Full blueprint JSON structure to persist.";
				regP("entity_template_editor.save",
				     "Serialises the given blueprint structure to disk at the specified path. The blueprint "
				     "parameter must be the full JSON structure as returned by load (or as modified by "
				     "update_field, add_component, remove_component). Overwrites the existing file.",
				     "entity_template_editor",
				     [this](const Json::Value& data) -> Json::Value {
				         if (!data.isMember("path") || !data["path"].isString() || !data.isMember("blueprint"))
				             return MakeErrorResponse("missing path or blueprint");
				         char resolvedPath[512] = {};
				         ResolvePath(data["path"].asCString(), resolvedPath, sizeof(resolvedPath));
				         char err[256] = {};
				         if (!mFileHandler.Save(resolvedPath, data["blueprint"], err, sizeof(err)))
				             return MakeErrorResponse(err[0] ? err : "save failed");
				         return MakeSuccessResponse();
				     },
				     params, 2);
			}

			// 7. update_field — delegates to existing WebUIBridge handler to avoid duplicating
			//    the multi-step load/mutate/save logic (safe: both paths are on kMainThread)
			{
				Dia::Editor::EditorActionParam params[4];
				params[0].name = "path";          params[0].type = "string"; params[0].required = true; params[0].description = "Absolute path to the .diablueprint file.";
				params[1].name = "componentType"; params[1].type = "string"; params[1].required = true; params[1].description = "Component type name, e.g. 'TransformComponent'.";
				params[2].name = "fieldName";     params[2].type = "string"; params[2].required = true; params[2].description = "Field name within the component.";
				params[3].name = "value";         params[3].type = "any";    params[3].required = true; params[3].description = "New value. Pass null to reset to component default.";
				regP("entity_template_editor.update_field",
				     "Updates a single component field in a blueprint file. Loads the file, applies the "
				     "change via BlueprintMutator, and saves immediately. Passing null as value removes "
				     "the field (sets it to the component default). The path + componentType + fieldName "
				     "triple uniquely identifies the target field.",
				     "entity_template_editor",
				     [this](const Json::Value& data) -> Json::Value {
				         return GetBridge()->InvokeRequestHandler(
				             Dia::Core::StringCRC("entity_template_editor.update_field"), data);
				     },
				     params, 4);
			}

			// 8. add_component — delegates to existing WebUIBridge handler
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name = "path";          params[0].type = "string"; params[0].required = true; params[0].description = "Absolute path to the .diablueprint file.";
				params[1].name = "componentType"; params[1].type = "string"; params[1].required = true; params[1].description = "Component type to add. Use get_available_components to enumerate valid types.";
				regP("entity_template_editor.add_component",
				     "Adds a component of the given type to the blueprint file. The component is initialised "
				     "with its schema defaults. Loads the file, applies the change via BlueprintMutator, and "
				     "saves immediately. Returns an error if the component type is already present or unknown.",
				     "entity_template_editor",
				     [this](const Json::Value& data) -> Json::Value {
				         return GetBridge()->InvokeRequestHandler(
				             Dia::Core::StringCRC("entity_template_editor.add_component"), data);
				     },
				     params, 2);
			}

			// 9. remove_component — delegates to existing WebUIBridge handler
			{
				Dia::Editor::EditorActionParam params[2];
				params[0].name = "path";          params[0].type = "string"; params[0].required = true; params[0].description = "Absolute path to the .diablueprint file.";
				params[1].name = "componentType"; params[1].type = "string"; params[1].required = true; params[1].description = "Component type to remove.";
				regP("entity_template_editor.remove_component",
				     "Removes a component from the blueprint file. All field values for that component are "
				     "discarded. Loads the file, applies the change via BlueprintMutator, and saves "
				     "immediately. Returns an error if the component type is not present.",
				     "entity_template_editor",
				     [this](const Json::Value& data) -> Json::Value {
				         return GetBridge()->InvokeRequestHandler(
				             Dia::Core::StringCRC("entity_template_editor.remove_component"), data);
				     },
				     params, 2);
			}

			DIA_LOG_INFO("Editor", "DiaEntityTemplateEditorPlugin: Dual-registered 9 entity_template_editor.* actions");
		}

	}
}
