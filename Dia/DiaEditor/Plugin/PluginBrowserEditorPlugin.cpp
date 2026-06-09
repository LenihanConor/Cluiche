#include "DiaEditor/Plugin/PluginBrowserEditorPlugin.h"
#include "DiaEditor/Plugin/EditorPluginRegistrationMacros.h"
#include "DiaEditor/Plugin/EditorPluginRegistry.h"
#include "DiaEditor/Plugin/IPluginLoader.h"
#include "DiaEditor/Plugin/EditorPluginBase.h"
#include "DiaEditor/Plugin/PluginServiceLocator.h"
#include "DiaEditor/EditorAPI/EditorActionRegistryService.h"
#include "DiaEditor/EditorAPI/EditorActionDescriptor.h"

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <algorithm>
#include <string>

namespace Dia
{
	namespace Editor
	{
		void PluginBrowserEditorPlugin::OnPluginLoad()
		{
			DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: OnPluginLoad");

			RegisterHandler(Dia::Core::StringCRC("plugin_browser.get_available"),
				[this](const Json::Value& /*data*/) -> Json::Value
				{
					Json::Value result;
					result["plugins"] = Json::arrayValue;

					EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
					for (unsigned int i = 0; i < registry.GetRegisteredCount(); ++i)
					{
						const Dia::Core::StringCRC& typeId = registry.GetRegisteredTypeId(i);
						if (!registry.IsInScopeFilter(typeId))
							continue;
						EditorPluginInfo info = registry.GetFactory(i)->GetPluginInfo();

						Json::Value entry;
						entry["name"] = info.name;
						entry["version"] = info.version;
						entry["description"] = info.description;
						entry["typeId"] = typeId.AsChar();
						entry["loaded"] = (GetPluginLoader() != nullptr) ? GetPluginLoader()->IsPluginTypeLoaded(typeId) : false;
						entry["pinned"] = (GetPluginLoader() != nullptr) ? GetPluginLoader()->IsPluginPinned(typeId) : false;

						result["plugins"].append(entry);
					}

					return result;
				});

			RegisterHandler(Dia::Core::StringCRC("plugin_browser.load"),
				[this](const Json::Value& data) -> Json::Value
				{
					const char* typeIdStr = data.isMember("typeId") ? data["typeId"].asCString() : nullptr;
					if (typeIdStr == nullptr || typeIdStr[0] == '\0')
						return MakeErrorResponse("typeId is required");

					Dia::Core::StringCRC typeId(typeIdStr);

					if (GetPluginLoader() == nullptr)
						return MakeErrorResponse("plugin loader not available");

					if (GetPluginLoader()->IsPluginTypeLoaded(typeId))
						return MakeErrorResponse("plugin is already loaded");

					if (!EditorPluginRegistry::Instance().IsPluginRegistered(typeId))
						return MakeErrorResponse("plugin type not registered");

					Dia::Core::StringCRC instanceId((std::string(typeIdStr) + "_browser").c_str());
					GetPluginLoader()->LoadPlugin(typeId, instanceId);

					return MakeSuccessResponse();
				});

			RegisterHandler(Dia::Core::StringCRC("plugin_browser.unload"),
				[this](const Json::Value& data) -> Json::Value
				{
					const char* typeIdStr = data.isMember("typeId") ? data["typeId"].asCString() : nullptr;
					if (typeIdStr == nullptr || typeIdStr[0] == '\0')
						return MakeErrorResponse("typeId is required");

					Dia::Core::StringCRC typeId(typeIdStr);

					if (GetPluginLoader() == nullptr)
						return MakeErrorResponse("plugin loader not available");

					if (!GetPluginLoader()->IsPluginTypeLoaded(typeId))
						return MakeErrorResponse("plugin is not loaded");

					if (GetPluginLoader()->IsPluginPinned(typeId))
						return MakeErrorResponse("plugin is pinned and cannot be unloaded");

					bool success = GetPluginLoader()->UnloadPlugin(typeId);
					if (!success)
						return MakeErrorResponse("failed to unload plugin");

					return MakeSuccessResponse();
				});

			DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: Registered request handlers");

			// Dual-register the generic load/unload actions in EditorActionRegistry so they
			// appear in the manifest and can be called via Python / DiaAPI without going through
			// the WebUIBridge. The handlers are identical to the WebUIBridge path above.
			if (GetServices() != nullptr)
			{
				Dia::Editor::EditorActionRegistryService* regSvc =
					GetServices()->GetService<Dia::Editor::EditorActionRegistryService>();
				if (regSvc != nullptr && regSvc->GetRegistry() != nullptr)
				{
					Dia::Editor::EditorActionRegistry* api = regSvc->GetRegistry();

					Dia::Editor::EditorActionDescriptor load;
					load.name           = Dia::Core::StringCRC("plugin_browser.load");
					load.description    = "Load an editor plugin by typeId string. Required param: typeId (string, e.g. 'SceneEditorPlugin'). Returns { ok: true } on success or { ok: false, error: string } if the plugin is already loaded, not registered, or the loader is unavailable. Fires DIA_LOG_INFO on load.";
					load.category       = "plugin";
					load.owner          = "PluginBrowserEditorPlugin";
					load.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
					load.handler        = [this](const Json::Value& data) -> Json::Value {
						const char* typeIdStr = data.isMember("typeId") ? data["typeId"].asCString() : nullptr;
						if (typeIdStr == nullptr || typeIdStr[0] == '\0')
							return MakeErrorResponse("typeId is required");
						Dia::Core::StringCRC typeId(typeIdStr);
						if (GetPluginLoader() == nullptr) return MakeErrorResponse("plugin loader not available");
						if (GetPluginLoader()->IsPluginTypeLoaded(typeId)) return MakeErrorResponse("plugin is already loaded");
						if (!EditorPluginRegistry::Instance().IsPluginRegistered(typeId)) return MakeErrorResponse("plugin type not registered");
						DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: loading plugin via action");
						Dia::Core::StringCRC instanceId((std::string(typeIdStr) + "_action").c_str());
						GetPluginLoader()->LoadPlugin(typeId, instanceId);
						return MakeSuccessResponse();
					};
					api->RegisterAction(load);

					Dia::Editor::EditorActionDescriptor unload;
					unload.name           = Dia::Core::StringCRC("plugin_browser.unload");
					unload.description    = "Unload a currently loaded editor plugin by typeId string. Required param: typeId (string). Returns { ok: true } on success or { ok: false, error: string } if the plugin is not loaded, is pinned, or unload fails. Fires DIA_LOG_INFO on unload.";
					unload.category       = "plugin";
					unload.owner          = "PluginBrowserEditorPlugin";
					unload.dispatchThread = Dia::Editor::DispatchThread::kMainThread;
					unload.handler        = [this](const Json::Value& data) -> Json::Value {
						const char* typeIdStr = data.isMember("typeId") ? data["typeId"].asCString() : nullptr;
						if (typeIdStr == nullptr || typeIdStr[0] == '\0')
							return MakeErrorResponse("typeId is required");
						Dia::Core::StringCRC typeId(typeIdStr);
						if (GetPluginLoader() == nullptr) return MakeErrorResponse("plugin loader not available");
						if (!GetPluginLoader()->IsPluginTypeLoaded(typeId)) return MakeErrorResponse("plugin is not loaded");
						if (GetPluginLoader()->IsPluginPinned(typeId)) return MakeErrorResponse("plugin is pinned and cannot be unloaded");
						DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: unloading plugin via action");
						bool success = GetPluginLoader()->UnloadPlugin(typeId);
						if (!success) return MakeErrorResponse("failed to unload plugin");
						return MakeSuccessResponse();
					};
					api->RegisterAction(unload);

					DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: Dual-registered load/unload actions in EditorActionRegistry");
				}
			}

			{
				EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
				IPluginLoader* loader = GetPluginLoader();
				for (unsigned int i = 0; i < registry.GetRegisteredCount(); ++i)
				{
					const Dia::Core::StringCRC typeId = registry.GetRegisteredTypeId(i);

					std::string typeIdLower(typeId.AsChar());
					std::transform(typeIdLower.begin(), typeIdLower.end(), typeIdLower.begin(), ::tolower);

					{
						std::string loadName = std::string("plugin.load.") + typeIdLower;
						Dia::API::CommandInfoJson loadCmd;
						loadCmd.name = Dia::Core::StringCRC(loadName.c_str());
						loadCmd.description = "Load plugin";
						loadCmd.category = Dia::Core::StringCRC("plugin");
						loadCmd.owner = "PluginBrowser";
						loadCmd.callback = [loader, typeId](const Json::Value&) -> Json::Value {
							if (loader != nullptr)
							{
								std::string instanceId = std::string(typeId.AsChar()) + "_palette";
								loader->LoadPlugin(typeId, Dia::Core::StringCRC(instanceId.c_str()));
							}
							return Json::Value(Json::objectValue);
						};
						Dia::API::RegisterCommandJson(loadCmd);
					}

					{
						std::string unloadName = std::string("plugin.unload.") + typeIdLower;
						Dia::API::CommandInfoJson unloadCmd;
						unloadCmd.name = Dia::Core::StringCRC(unloadName.c_str());
						unloadCmd.description = "Unload plugin";
						unloadCmd.category = Dia::Core::StringCRC("plugin");
						unloadCmd.owner = "PluginBrowser";
						unloadCmd.callback = [loader, typeId](const Json::Value&) -> Json::Value {
							if (loader != nullptr)
							{
								loader->UnloadPlugin(typeId);
							}
							return Json::Value(Json::objectValue);
						};
						Dia::API::RegisterCommandJson(unloadCmd);
					}
				}
				DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: Registered plugin load/unload commands");
			}
		}
	}
}

using namespace Dia::Editor;

REGISTER_EDITOR_PLUGIN(PluginBrowserEditorPlugin, "PluginBrowserEditorPlugin")
