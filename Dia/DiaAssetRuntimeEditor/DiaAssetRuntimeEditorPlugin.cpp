#include "DiaAssetRuntimeEditor/DiaAssetRuntimeEditorPlugin.h"
#include "DiaAssetRuntimeEditor/SharedPluginState.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			void DiaAssetRuntimeEditorPlugin::OnLoad(const Dia::Editor::EditorPluginContext& context)
			{
				DIA_LOG_INFO("Editor", "DiaAssetRuntimeEditorPlugin: OnLoad");

				mBridge = context.mBridge;
				mView = context.mView;
				mPluginLoader = context.mPluginLoader;

				mState = new SharedPluginState();

				mManager.Initialize();
				mManager.SetConnectionCallback(
					[this](bool connected) { HandleConnectionStateChange(connected); });

				mAssetStateTable.Activate(mBridge, &mManager, mState);
				mStageAssetTree.Activate(mBridge, &mManager, mState, &mAssetStateTable);
				mRefCountInspector.Activate(mBridge, &mManager, mState, &mAssetStateTable);
				mTransitionLog.Activate(mBridge, &mManager, mState);

				RegisterRequestHandlers();

				DIA_LOG_INFO("Editor", "DiaAssetRuntimeEditorPlugin: Initialized");
			}

			void DiaAssetRuntimeEditorPlugin::OnUnload()
			{
				DIA_LOG_INFO("Editor", "DiaAssetRuntimeEditorPlugin: OnUnload");

				mTransitionLog.Deactivate();
				mRefCountInspector.Deactivate();
				mStageAssetTree.Deactivate();
				mAssetStateTable.Deactivate();
				mManager.Shutdown();

				delete mState;
				mState = nullptr;

				mBridge = nullptr;
				mView = nullptr;
				mPluginLoader = nullptr;
			}

			void DiaAssetRuntimeEditorPlugin::OnUpdate(float deltaTime)
			{
				mManager.Update(deltaTime);
				mAssetStateTable.Update(deltaTime);
				mStageAssetTree.Update(deltaTime);
				mRefCountInspector.Update(deltaTime);
				mTransitionLog.Update(deltaTime);
			}

			SharedPluginState* DiaAssetRuntimeEditorPlugin::GetPluginData()
			{
				return mState;
			}

			void DiaAssetRuntimeEditorPlugin::RegisterRequestHandlers()
			{
				if (!mBridge)
					return;

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_runtime_editor.connect"),
					[this](const Json::Value& data) -> Json::Value
					{
						Json::Value result;
						if (!data.isMember("host") || !data.isMember("port"))
						{
							result["success"] = false;
							result["error"] = "missing host or port";
							return result;
						}
						const char* host = data["host"].asCString();
						int port = data["port"].asInt();
						mManager.Connect(host, port);
						result["success"] = true;
						return result;
					});

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_runtime_editor.disconnect"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						mManager.Disconnect();
						Json::Value result;
						result["success"] = true;
						return result;
					});
			}

			void DiaAssetRuntimeEditorPlugin::HandleConnectionStateChange(bool connected)
			{
				if (mState)
					mState->mConnected = connected;

				mAssetStateTable.OnConnectionStateChanged(connected);
				mStageAssetTree.OnConnectionStateChanged(connected);
				mRefCountInspector.OnConnectionStateChanged(connected);
				mTransitionLog.OnConnectionStateChanged(connected);

				if (mBridge)
				{
					Json::Value data;
					data["connected"] = connected;
					mBridge->NotifyUIDataChanged("asset_runtime_editor.connection_state", data);
				}

				if (connected)
					DIA_LOG_INFO("Editor", "DiaAssetRuntimeEditorPlugin: Connected to game");
				else
					DIA_LOG_INFO("Editor", "DiaAssetRuntimeEditorPlugin: Disconnected from game");
			}
		}
	}
}

using namespace Dia::AssetRuntime::Editor;

REGISTER_EDITOR_PLUGIN(DiaAssetRuntimeEditorPlugin, "DiaAssetRuntimeEditorPlugin")
