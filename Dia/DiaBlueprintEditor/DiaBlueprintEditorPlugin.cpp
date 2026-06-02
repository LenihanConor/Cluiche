#include "DiaBlueprintEditor/DiaBlueprintEditorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaAssetCatalogue/AssetRecord.h>
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

			if (self->mBridge)
				self->mBridge->NotifyUIDataChanged("blueprint_editor.project_changed", Json::Value(ctx.diagamePath));
		}

		void DiaBlueprintEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
		{
			DIA_LOG_INFO("Editor", "DiaBlueprintEditorPlugin: OnLoad");

			mBridge       = context.mBridge;
			mPluginLoader = context.mPluginLoader;

			RegisterRequestHandlers();

			if (context.mModel != nullptr)
				context.mModel->OnDiagameProjectChanged(&DiaBlueprintEditorPlugin::OnProjectChangedStatic, this);
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
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.get_list"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.load"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.save"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.get_available_components"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.add_component"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.remove_component"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.update_field"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.get_usage"));
				mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("blueprint_editor.register_catalogue_asset"));
			}

			mBridge       = nullptr;
			mPluginLoader = nullptr;
		}

		void DiaBlueprintEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
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
		void DiaBlueprintEditorPlugin::RegisterRequestHandlers()
		{
			if (!mBridge)
				return;

			RegisterListHandlers();
			RegisterPropertyHandlers();
			RegisterFileHandlers();
			RegisterAssetTypeHandlers();
		}

		void DiaBlueprintEditorPlugin::RegisterListHandlers()
		{
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_list"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					DIA_TRACE_ZONE("blueprint_editor.get_list", Dia::Observation::Trace::Category::kNone);
					return mListController.BuildListJson(mRegistry);
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
					result["components"] = mPropertyController.BuildAvailableComponentsJson(blueprintRoot, topKey);
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

					Dia::Core::StringCRC assetId(data["assetId"].asCString());
					result["success"] = true;
					result["usage"]   = mPropertyController.BuildUsageJson(assetId, mRegistry);
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

					if (!blueprintRoot.isMember(topKey))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — top-level key '%s' not found in '%s'",
							topKey, data["path"].asCString());
						result["success"] = false;
						result["error"]   = "blueprint root key not found";
						return result;
					}

					const char* componentType = data["componentType"].asCString();
					const char* fieldName     = data["fieldName"].asCString();
					Json::Value& components   = blueprintRoot[topKey]["components"];

					bool found = false;
					for (unsigned int i = 0; i < components.size(); ++i)
					{
						if (components[i]["type"].asString() == componentType)
						{
							components[i]["fields"][fieldName] = data["value"];
							found = true;
							break;
						}
					}

					if (!found)
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: update_field — component '%s' not found in '%s'",
							componentType, data["path"].asCString());
						result["success"] = false;
						result["error"]   = "component type not found";
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

					if (!blueprintRoot.isMember(topKey))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: add_component — top-level key '%s' not found in '%s'",
							topKey, data["path"].asCString());
						result["success"] = false;
						result["error"]   = "blueprint root key not found";
						return result;
					}

					const char* componentType = data["componentType"].asCString();
					Json::Value& components   = blueprintRoot[topKey]["components"];

					for (unsigned int i = 0; i < components.size(); ++i)
					{
						if (components[i]["type"].asString() == componentType)
						{
							DIA_LOG_WARNING("Editor",
								"DiaBlueprintEditorPlugin: add_component — type '%s' already present in '%s'",
								componentType, data["path"].asCString());
							result["success"] = false;
							result["error"]   = "component type already present";
							return result;
						}
					}

					Json::Value newComp;
					newComp["type"]   = componentType;
					newComp["fields"] = Json::Value(Json::objectValue);
					components.append(newComp);

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
						componentType, data["path"].asCString());
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

					if (!blueprintRoot.isMember(topKey))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — top-level key '%s' not found in '%s'",
							topKey, data["path"].asCString());
						result["success"] = false;
						result["error"]   = "blueprint root key not found";
						return result;
					}

					const char* componentType = data["componentType"].asCString();
					Json::Value& components   = blueprintRoot[topKey]["components"];
					Json::Value  newComponents(Json::arrayValue);

					bool found = false;
					for (unsigned int i = 0; i < components.size(); ++i)
					{
						if (components[i]["type"].asString() == componentType)
							found = true;
						else
							newComponents.append(components[i]);
					}

					if (!found)
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: remove_component — type '%s' not found in '%s'",
							componentType, data["path"].asCString());
						result["success"] = false;
						result["error"]   = "component type not found";
						return result;
					}

					blueprintRoot[topKey]["components"] = newComponents;

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
						componentType, data["path"].asCString());
					result["success"] = true;
					return result;
				});
		}

		void DiaBlueprintEditorPlugin::RegisterAssetTypeHandlers()
		{
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.register_catalogue_asset"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("id") || !data["id"].isString()
					    || !data.isMember("typeId") || !data["typeId"].isString()
					    || !data.isMember("sourcePath") || !data["sourcePath"].isString())
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: register_catalogue_asset — missing id, typeId, or sourcePath");
						result["success"] = false;
						result["error"]   = "missing id, typeId, or sourcePath";
						return result;
					}

					Dia::AssetCatalogue::AssetRecord rec;
					rec.mId          = Dia::Core::StringCRC(data["id"].asCString());
					rec.mAssetTypeId = Dia::Core::StringCRC(data["typeId"].asCString());
					rec.mSourcePath  = data["sourcePath"].asCString();

					if (!BlueprintListController::IsBlueprintType(rec.mAssetTypeId))
					{
						DIA_LOG_WARNING("Editor",
							"DiaBlueprintEditorPlugin: register_catalogue_asset — '%s' is not a blueprint type",
							data["typeId"].asCString());
						result["success"] = false;
						result["error"]   = "not a blueprint asset type";
						return result;
					}

					mRegistry.Remove(rec.mId);
					mRegistry.Register(rec);

					DIA_LOG_INFO("Editor",
						"DiaBlueprintEditorPlugin: registered catalogue asset '%s' (type '%s')",
						data["id"].asCString(), data["typeId"].asCString());
					result["success"] = true;
					return result;
				});
		}
	}
}
