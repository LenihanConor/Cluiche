#include "DiaAssetRuntimeInspector/DiaAssetRuntimeInspectorPlugin.h"
#include "DiaAssetRuntimeInspector/SharedPluginState.h"

#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstring>

static const char* kDefaultOutputDir = "Cluiche/out/CluicheEditor/DiaAssetRuntimeInspector";

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Inspector
		{
			void DiaAssetRuntimeInspectorPlugin::OnProjectChanged(const Dia::Editor::ProjectContext& ctx)
			{
				strncpy_s(mExpectedDiagamePath, sizeof(mExpectedDiagamePath),
				          ctx.IsValid() ? ctx.diagamePath : "", _TRUNCATE);
			}

			void DiaAssetRuntimeInspectorPlugin::OnPluginLoad()
			{
				DIA_LOG_INFO("Editor", "DiaAssetRuntimeInspectorPlugin: OnPluginLoad");

				if (GetModel() != nullptr)
				{
					const Dia::Editor::ProjectContext& proj = GetModel()->GetDiagameProject();
					strncpy_s(mExpectedDiagamePath, sizeof(mExpectedDiagamePath),
					          proj.IsValid() ? proj.diagamePath : "", _TRUNCATE);
				}

				mState = std::make_unique<SharedPluginState>();

				if (GetServices())
					mManager = GetServices()->GetService<Dia::Editor::GameConnectionManager>();

				if (!mManager)
					DIA_LOG_WARNING("Editor", "DiaAssetRuntimeInspectorPlugin: GameConnectionManager service not available");

				mSessionContext.Load(kDefaultOutputDir);

				mAssetStateTable.Activate(GetBridge(), mManager, mState.get());
				mAssetStateTable.SetPollInterval(mSessionContext.GetPollInterval());
				mStageAssetTree.Activate(GetBridge(), mManager, mState.get(), &mAssetStateTable);
				mRefCountInspector.Activate(GetBridge(), mManager, mState.get(), &mAssetStateTable);
				mTransitionLog.Activate(GetBridge(), mManager, mState.get());
				mTransitionLog.SetMaxEntries(mSessionContext.GetMaxLogEntries());

				if (mSessionContext.GetStateFilter())
					strncpy_s(mCurrentStateFilter, mSessionContext.GetStateFilter(), _TRUNCATE);
				if (mSessionContext.GetIdSearchText())
					strncpy_s(mCurrentIdSearch, mSessionContext.GetIdSearchText(), _TRUNCATE);

				RegisterHandler(
					Dia::Core::StringCRC("asset_runtime_inspector.get_connection_state"),
					[this](const Json::Value& /*data*/) -> Json::Value
					{
						Json::Value result;
						result["connected"] = (mManager != nullptr && mManager->IsConnected());
						return result;
					});

				RegisterHandler(
					Dia::Core::StringCRC("asset_runtime_inspector.update_filters"),
					[this](const Json::Value& data) -> Json::Value
					{
						if (data.isMember("stateFilter") && data["stateFilter"].isString())
							strncpy_s(mCurrentStateFilter, data["stateFilter"].asCString(), _TRUNCATE);
						if (data.isMember("idSearch") && data["idSearch"].isString())
							strncpy_s(mCurrentIdSearch, data["idSearch"].asCString(), _TRUNCATE);
						return MakeSuccessResponse();
					});

				PushSavedFiltersToUI();

				if (mManager && mManager->IsConnected())
					HandleConnectionStateChange(true);

				DIA_LOG_INFO("Editor", "DiaAssetRuntimeInspectorPlugin: Initialized");
			}

			void DiaAssetRuntimeInspectorPlugin::PushSavedFiltersToUI()
			{
				if (!GetBridge())
					return;

				const char* stateFilter = mSessionContext.GetStateFilter();
				const char* idSearch = mSessionContext.GetIdSearchText();

				if ((stateFilter && stateFilter[0] != '\0') || (idSearch && idSearch[0] != '\0'))
				{
					Json::Value data;
					data["stateFilter"] = stateFilter ? stateFilter : "";
					data["idSearch"] = idSearch ? idSearch : "";
					GetBridge()->NotifyUIDataChanged("asset_runtime_inspector.table_filters", data);
				}
			}

			void DiaAssetRuntimeInspectorPlugin::OnPluginUnload()
			{
				DIA_LOG_INFO("Editor", "DiaAssetRuntimeInspectorPlugin: OnPluginUnload");

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
			}

			void DiaAssetRuntimeInspectorPlugin::OnUpdate(float deltaTime)
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

			SharedPluginState* DiaAssetRuntimeInspectorPlugin::GetPluginData()
			{
				return mState.get();
			}

			void DiaAssetRuntimeInspectorPlugin::SaveCurrentFilters()
			{
				mSessionContext.SetStateFilter(mCurrentStateFilter);
				mSessionContext.SetIdSearchText(mCurrentIdSearch);
			}

			void DiaAssetRuntimeInspectorPlugin::HandleConnectionStateChange(bool connected)
			{
				if (mState)
					mState->mConnected = connected;

				mAssetStateTable.OnConnectionStateChanged(connected);
				mStageAssetTree.OnConnectionStateChanged(connected);
				mRefCountInspector.OnConnectionStateChanged(connected);
				mTransitionLog.OnConnectionStateChanged(connected);

				if (GetBridge())
				{
					Json::Value data;
					data["connected"] = connected;
					GetBridge()->NotifyUIDataChanged("asset_runtime_inspector.connection_state", data);
				}

				if (connected)
					DIA_LOG_INFO("Editor", "DiaAssetRuntimeInspectorPlugin: Connected to game");
				else
					DIA_LOG_INFO("Editor", "DiaAssetRuntimeInspectorPlugin: Disconnected from game");
			}
		}
	}
}

using namespace Dia::AssetRuntime::Inspector;

REGISTER_EDITOR_PLUGIN(DiaAssetRuntimeInspectorPlugin, "DiaAssetRuntimeInspectorPlugin")
