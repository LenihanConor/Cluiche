#include "DiaAssetRuntimeEditor/DiaAssetRuntimeEditorPlugin.h"
#include "DiaAssetRuntimeEditor/SharedPluginState.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaLogger/DiaLog.h>

#include <cstring>

static const char* kDefaultOutputDir = "Cluiche/out/CluicheEditor/DiaAssetRuntimeEditor";

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

				mState = std::make_unique<SharedPluginState>();

				mManager.Initialize();
				mManager.SetConnectionCallback(
					[this](bool connected) { HandleConnectionStateChange(connected); });

				mSessionContext.Load(kDefaultOutputDir);

				mAssetStateTable.Activate(mBridge, &mManager, mState.get());
				mAssetStateTable.SetPollInterval(mSessionContext.GetPollInterval());
				mStageAssetTree.Activate(mBridge, &mManager, mState.get(), &mAssetStateTable);
				mRefCountInspector.Activate(mBridge, &mManager, mState.get(), &mAssetStateTable);
				mTransitionLog.Activate(mBridge, &mManager, mState.get());
				mTransitionLog.SetMaxEntries(mSessionContext.GetMaxLogEntries());

				if (mSessionContext.GetStateFilter())
					strncpy_s(mCurrentStateFilter, mSessionContext.GetStateFilter(), _TRUNCATE);
				if (mSessionContext.GetIdSearchText())
					strncpy_s(mCurrentIdSearch, mSessionContext.GetIdSearchText(), _TRUNCATE);

				RegisterRequestHandlers();
				PushSavedFiltersToUI();

				DIA_LOG_INFO("Editor", "DiaAssetRuntimeEditorPlugin: Initialized");
			}

			void DiaAssetRuntimeEditorPlugin::PushSavedFiltersToUI()
			{
				if (!mBridge)
					return;

				const char* stateFilter = mSessionContext.GetStateFilter();
				const char* idSearch = mSessionContext.GetIdSearchText();

				if ((stateFilter && stateFilter[0] != '\0') || (idSearch && idSearch[0] != '\0'))
				{
					Json::Value data;
					data["stateFilter"] = stateFilter ? stateFilter : "";
					data["idSearch"] = idSearch ? idSearch : "";
					mBridge->NotifyUIDataChanged("asset_runtime_editor.table_filters", data);
				}
			}

			void DiaAssetRuntimeEditorPlugin::OnUnload()
			{
				DIA_LOG_INFO("Editor", "DiaAssetRuntimeEditorPlugin: OnUnload");

				mSessionContext.SetPollInterval(mAssetStateTable.GetPollInterval());
				mSessionContext.SetMaxLogEntries(mTransitionLog.GetMaxEntries());
				SaveCurrentFilters();
				mSessionContext.Save(kDefaultOutputDir);

				mTransitionLog.Deactivate();
				mRefCountInspector.Deactivate();
				mStageAssetTree.Deactivate();
				mAssetStateTable.Deactivate();
				mManager.Shutdown();

				mState.reset();

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
				return mState.get();
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

				mBridge->RegisterRequestHandler(
					Dia::Core::StringCRC("asset_runtime_editor.update_filters"),
					[this](const Json::Value& data) -> Json::Value
					{
						if (data.isMember("stateFilter") && data["stateFilter"].isString())
							strncpy_s(mCurrentStateFilter, data["stateFilter"].asCString(), _TRUNCATE);
						if (data.isMember("idSearch") && data["idSearch"].isString())
							strncpy_s(mCurrentIdSearch, data["idSearch"].asCString(), _TRUNCATE);
						Json::Value result;
						result["success"] = true;
						return result;
					});
			}

			void DiaAssetRuntimeEditorPlugin::SaveCurrentFilters()
			{
				mSessionContext.SetStateFilter(mCurrentStateFilter);
				mSessionContext.SetIdSearchText(mCurrentIdSearch);
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
