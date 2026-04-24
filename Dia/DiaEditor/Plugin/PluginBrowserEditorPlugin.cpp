#include "DiaEditor/Plugin/PluginBrowserEditorPlugin.h"
#include "DiaEditor/Plugin/EditorPluginRegistrationMacros.h"
#include "DiaEditor/Plugin/EditorPluginContext.h"
#include "DiaEditor/Plugin/EditorPluginRegistry.h"
#include "DiaEditor/Plugin/IPluginLoader.h"
#include "DiaEditor/UI/WebUIBridge.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <string>

namespace Dia
{
	namespace Editor
	{
		static const Dia::Core::StringCRC kReqGetAvailable("plugin_browser.get_available");
		static const Dia::Core::StringCRC kReqLoadPlugin("plugin_browser.load");
		static const Dia::Core::StringCRC kReqUnloadPlugin("plugin_browser.unload");

		void PluginBrowserEditorPlugin::OnLoad(const EditorPluginContext& context)
		{
			DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: OnLoad");
			mBridge = context.mBridge;
			mPluginLoader = context.mPluginLoader;

			if (mBridge != nullptr)
			{
				mBridge->RegisterRequestHandler(kReqGetAvailable,
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						result["plugins"] = Json::arrayValue;

						EditorPluginRegistry& registry = EditorPluginRegistry::Instance();
						for (unsigned int i = 0; i < registry.GetRegisteredCount(); ++i)
						{
							const Dia::Core::StringCRC& typeId = registry.GetRegisteredTypeId(i);
							EditorPluginInfo info = registry.GetFactory(i)->GetPluginInfo();

							Json::Value entry;
							entry["name"] = info.name;
							entry["version"] = info.version;
							entry["description"] = info.description;
							entry["typeId"] = typeId.AsChar();
							entry["loaded"] = (mPluginLoader != nullptr) ? mPluginLoader->IsPluginTypeLoaded(typeId) : false;
							entry["pinned"] = (mPluginLoader != nullptr) ? mPluginLoader->IsPluginPinned(typeId) : false;

							result["plugins"].append(entry);
						}

						return result;
					});

				mBridge->RegisterRequestHandler(kReqLoadPlugin,
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;

						const char* typeIdStr = data.isMember("typeId") ? data["typeId"].asCString() : nullptr;
						if (typeIdStr == nullptr || typeIdStr[0] == '\0')
						{
							result["ok"] = false;
							result["error"] = "typeId is required";
							return result;
						}

						Dia::Core::StringCRC typeId(typeIdStr);

						if (mPluginLoader == nullptr)
						{
							result["ok"] = false;
							result["error"] = "plugin loader not available";
							return result;
						}

						if (mPluginLoader->IsPluginTypeLoaded(typeId))
						{
							result["ok"] = false;
							result["error"] = "plugin is already loaded";
							return result;
						}

						if (!EditorPluginRegistry::Instance().IsPluginRegistered(typeId))
						{
							result["ok"] = false;
							result["error"] = "plugin type not registered";
							return result;
						}

						Dia::Core::StringCRC instanceId((std::string(typeIdStr) + "_browser").c_str());
						mPluginLoader->LoadPlugin(typeId, instanceId);

						result["ok"] = true;
						return result;
					});

				mBridge->RegisterRequestHandler(kReqUnloadPlugin,
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;

						const char* typeIdStr = data.isMember("typeId") ? data["typeId"].asCString() : nullptr;
						if (typeIdStr == nullptr || typeIdStr[0] == '\0')
						{
							result["ok"] = false;
							result["error"] = "typeId is required";
							return result;
						}

						Dia::Core::StringCRC typeId(typeIdStr);

						if (mPluginLoader == nullptr)
						{
							result["ok"] = false;
							result["error"] = "plugin loader not available";
							return result;
						}

						if (!mPluginLoader->IsPluginTypeLoaded(typeId))
						{
							result["ok"] = false;
							result["error"] = "plugin is not loaded";
							return result;
						}

						if (mPluginLoader->IsPluginPinned(typeId))
						{
							result["ok"] = false;
							result["error"] = "plugin is pinned and cannot be unloaded";
							return result;
						}

						bool success = mPluginLoader->UnloadPlugin(typeId);
						result["ok"] = success;
						if (!success)
						{
							result["error"] = "failed to unload plugin";
						}
						return result;
					});

				DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: Registered request handlers");
			}
		}

		void PluginBrowserEditorPlugin::OnUnload()
		{
			DIA_LOG_INFO("Editor", "PluginBrowserEditorPlugin: OnUnload");
			if (mBridge != nullptr)
			{
				mBridge->UnregisterRequestHandler(kReqGetAvailable);
				mBridge->UnregisterRequestHandler(kReqLoadPlugin);
				mBridge->UnregisterRequestHandler(kReqUnloadPlugin);
				mBridge = nullptr;
			}
		}

		void PluginBrowserEditorPlugin::OnUpdate(float /*deltaTime*/)
		{
		}
	}
}

using namespace Dia::Editor;

REGISTER_EDITOR_PLUGIN(PluginBrowserEditorPlugin, "PluginBrowserEditorPlugin")
