#include "DiaBlueprintEditor/DiaBlueprintEditorPlugin.h"
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaAssetCatalogue/AssetRecord.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>
#include <cstring>

using namespace Dia::BlueprintEditor;

REGISTER_EDITOR_PLUGIN(DiaBlueprintEditorPlugin, "DiaBlueprintEditor")

namespace Dia
{
	namespace BlueprintEditor
	{
		// Known blueprint asset type IDs as string constants.
		static const char* kDiaEntityTypeId  = "diaentity";
		static const char* kDiaCameraTypeId  = "diacamera";
		static const char* kDiaLightTypeId   = "dialight";

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
			// blueprint_editor.get_list — returns all blueprint assets grouped by type
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_list"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					return mListController.BuildListJson(mRegistry);
				});
		}

		void DiaBlueprintEditorPlugin::RegisterPropertyHandlers()
		{
			// blueprint_editor.get_available_components — component types not yet in the blueprint
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_available_components"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						result["success"] = false;
						result["error"]   = "missing path";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");
					result["success"]    = true;
					result["components"] = mPropertyController.BuildAvailableComponentsJson(blueprintRoot, topKey);
					return result;
				});

			// blueprint_editor.get_usage — scenes + instance counts referencing this blueprint
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.get_usage"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("assetId") || !data["assetId"].isString())
					{
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
			// blueprint_editor.load — load a blueprint file and return its property JSON
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.load"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString())
					{
						result["success"] = false;
						result["error"]   = "missing path";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					result["success"]    = true;
					result["properties"] = mPropertyController.BuildPropertyJson(blueprintRoot, topKey);
					return result;
				});

			// blueprint_editor.save — save a blueprint root back to file
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.save"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("blueprint"))
					{
						result["success"] = false;
						result["error"]   = "missing path or blueprint";
						return result;
					}

					char err[256] = {};
					if (!mFileHandler.Save(data["path"].asCString(), data["blueprint"], err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					result["success"] = true;
					return result;
				});

			// blueprint_editor.update_field — patch one field value and save
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.update_field"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data.isMember("fieldName")
					    || !data.isMember("value"))
					{
						result["success"] = false;
						result["error"]   = "missing path, componentType, fieldName, or value";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!blueprintRoot.isMember(topKey))
					{
						result["success"] = false;
						result["error"]   = "blueprint root key not found";
						return result;
					}

					const char* componentType = data["componentType"].asCString();
					const char* fieldName     = data["fieldName"].asCString();
					Json::Value& components   = blueprintRoot[topKey]["components"];

					for (unsigned int i = 0; i < components.size(); ++i)
					{
						if (components[i]["type"].asString() == componentType)
						{
							components[i]["fields"][fieldName] = data["value"];
							break;
						}
					}

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					result["success"] = true;
					return result;
				});

			// blueprint_editor.add_component — add a component with zero-defaults
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.add_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						result["success"] = false;
						result["error"]   = "missing path or componentType";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!blueprintRoot.isMember(topKey))
					{
						result["success"] = false;
						result["error"]   = "blueprint root key not found";
						return result;
					}

					const char* componentType = data["componentType"].asCString();

					// Check not already present
					Json::Value& components = blueprintRoot[topKey]["components"];
					for (unsigned int i = 0; i < components.size(); ++i)
					{
						if (components[i]["type"].asString() == componentType)
						{
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
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					result["success"] = true;
					return result;
				});

			// blueprint_editor.remove_component — remove a component by type
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.remove_component"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("path") || !data["path"].isString()
					    || !data.isMember("componentType") || !data["componentType"].isString())
					{
						result["success"] = false;
						result["error"]   = "missing path or componentType";
						return result;
					}

					Json::Value blueprintRoot;
					char err[256] = {};
					if (!mFileHandler.Load(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "load failed";
						return result;
					}

					const char* ext = strrchr(data["path"].asCString(), '.');
					const char* topKey = BlueprintFileHandler::TopLevelKeyForExtension(ext ? ext : "");

					if (!blueprintRoot.isMember(topKey))
					{
						result["success"] = false;
						result["error"]   = "blueprint root key not found";
						return result;
					}

					const char* componentType = data["componentType"].asCString();
					Json::Value& components   = blueprintRoot[topKey]["components"];
					Json::Value newComponents(Json::arrayValue);

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
						result["success"] = false;
						result["error"]   = "component type not found";
						return result;
					}

					blueprintRoot[topKey]["components"] = newComponents;

					if (!mFileHandler.Save(data["path"].asCString(), blueprintRoot, err, sizeof(err)))
					{
						result["success"] = false;
						result["error"]   = err[0] ? err : "save failed";
						return result;
					}

					result["success"] = true;
					return result;
				});
		}

		void DiaBlueprintEditorPlugin::RegisterAssetTypeHandlers()
		{
			// blueprint_editor.register_catalogue_asset — ingest an asset record from the catalogue
			// so the list panel shows up-to-date blueprints without a full project reload.
			mBridge->RegisterRequestHandler(
				Dia::Core::StringCRC("blueprint_editor.register_catalogue_asset"),
				[this](const Json::Value& data) -> Json::Value
				{
					Json::Value result;
					if (!data.isMember("id") || !data["id"].isString()
					    || !data.isMember("typeId") || !data["typeId"].isString()
					    || !data.isMember("sourcePath") || !data["sourcePath"].isString())
					{
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
						result["success"] = false;
						result["error"]   = "not a blueprint asset type";
						return result;
					}

					// idempotent: remove then re-register
					mRegistry.Remove(rec.mId);
					mRegistry.Register(rec);

					result["success"] = true;
					return result;
				});
		}
	}
}
