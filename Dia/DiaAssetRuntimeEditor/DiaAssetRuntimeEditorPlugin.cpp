#include "DiaAssetRuntimeEditor/DiaAssetRuntimeEditorPlugin.h"
#include "DiaAssetRuntimeEditor/SharedPluginState.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
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

				if (context.mServices)
					mManager = context.mServices->GetService<Dia::Editor::GameConnectionManager>();

				if (!mManager)
					DIA_LOG_WARNING("Editor", "DiaAssetRuntimeEditorPlugin: GameConnectionManager service not available");

				mSessionContext.Load(kDefaultOutputDir);

				mAssetStateTable.Activate(mBridge, mManager, mState.get());
				mAssetStateTable.SetPollInterval(mSessionContext.GetPollInterval());
				mStageAssetTree.Activate(mBridge, mManager, mState.get(), &mAssetStateTable);
				mRefCountInspector.Activate(mBridge, mManager, mState.get(), &mAssetStateTable);
				mTransitionLog.Activate(mBridge, mManager, mState.get());
				mTransitionLog.SetMaxEntries(mSessionContext.GetMaxLogEntries());

				if (mSessionContext.GetStateFilter())
					strncpy_s(mCurrentStateFilter, mSessionContext.GetStateFilter(), _TRUNCATE);
				if (mSessionContext.GetIdSearchText())
					strncpy_s(mCurrentIdSearch, mSessionContext.GetIdSearchText(), _TRUNCATE);

				RegisterRequestHandlers();
				PushSavedFiltersToUI();

				if (mManager && mManager->IsConnected())
					HandleConnectionStateChange(true);

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

				mState.reset();
				mManager = nullptr;

				mBridge = nullptr;
				mView = nullptr;
				mPluginLoader = nullptr;
			}

			void DiaAssetRuntimeEditorPlugin::OnUpdate(float deltaTime)
			{
				if (mManager)
				{
					bool isConnected = mManager->IsConnected();
					if (isConnected != mWasConnected)
					{
						mWasConnected = isConnected;
						HandleConnectionStateChange(isConnected);
					}
				}

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
					Dia::Core::StringCRC("asset_runtime_editor.get_connection_state"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						result["connected"] = (mManager != nullptr && mManager->IsConnected());
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
