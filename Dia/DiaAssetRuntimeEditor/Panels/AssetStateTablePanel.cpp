#include "DiaAssetRuntimeEditor/Panels/AssetStateTablePanel.h"
#include "DiaAssetRuntimeEditor/SharedPluginState.h"

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			const float AssetStateTablePanel::kDefaultPollIntervalSeconds = 1.0f;

			AssetStateTablePanel::AssetStateTablePanel()
				: mBridge(nullptr)
				, mManager(nullptr)
				, mState(nullptr)
				, mPollInterval(kDefaultPollIntervalSeconds)
				, mPollTimer(0.0f)
				, mActive(false)
				, mConnected(false)
			{}

			void AssetStateTablePanel::Activate(Dia::Editor::WebUIBridge* bridge,
				Dia::Editor::GameConnectionManager* manager,
				SharedPluginState* state)
			{
				mBridge = bridge;
				mManager = manager;
				mState = state;
				mActive = true;
				mPollTimer = 0.0f;

				if (mBridge)
				{
					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.force_refresh"),
						[this](const Json::Value& /*data*/) -> Json::Value
						{
							ForceRefresh();
							Json::Value result;
							result["success"] = true;
							return result;
						});

					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.set_poll_interval"),
						[this](const Json::Value& data) -> Json::Value
						{
							Json::Value result;
							if (data.isMember("interval") && data["interval"].isNumeric())
							{
								SetPollInterval(data["interval"].asFloat());
								result["success"] = true;
							}
							else
							{
								result["success"] = false;
								result["error"] = "missing interval";
							}
							return result;
						});

					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.select_asset"),
						[this](const Json::Value& data) -> Json::Value
						{
							Json::Value result;
							if (data.isMember("assetId") && data["assetId"].isString())
							{
								if (mState)
									mState->mSelectedAssetId = Dia::Core::StringCRC(data["assetId"].asCString());
								result["success"] = true;
							}
							else
							{
								result["success"] = false;
								result["error"] = "missing assetId";
							}
							return result;
						});
				}
			}

			void AssetStateTablePanel::Deactivate()
			{
				if (mBridge)
				{
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.force_refresh"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.set_poll_interval"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.select_asset"));
				}

				mActive = false;
				mBridge = nullptr;
				mManager = nullptr;
				mState = nullptr;
			}

			void AssetStateTablePanel::Update(float deltaTime)
			{
				if (!mActive || !mConnected)
					return;

				mPollTimer += deltaTime;
				if (mPollTimer >= mPollInterval)
				{
					mPollTimer = 0.0f;
					Poll();
				}
			}

			void AssetStateTablePanel::OnConnectionStateChanged(bool connected)
			{
				mConnected = connected;

				if (mBridge)
				{
					Json::Value data;
					data["connected"] = connected;
					mBridge->NotifyUIDataChanged("asset_runtime_editor.table_connection_state", data);
				}

				if (connected)
				{
					mPollTimer = 0.0f;
					Poll();
				}
				else
				{
					mSnapshot.RemoveAll();
					PushSnapshotToUI();
				}
			}

			void AssetStateTablePanel::ForceRefresh()
			{
				if (mConnected)
				{
					mPollTimer = 0.0f;
					Poll();
				}
			}

			void AssetStateTablePanel::SetPollInterval(float seconds)
			{
				if (seconds < 0.1f)
					seconds = 0.1f;
				mPollInterval = seconds;
			}

			void AssetStateTablePanel::Poll()
			{
				if (!mManager)
					return;

				Json::Value args(Json::objectValue);
				mManager->SendCommandWithResponse("asset_runtime.get_all_states", args,
					[this](bool success, const Json::Value& result)
					{
						HandlePollResponse(success, result);
					});
			}

			void AssetStateTablePanel::HandlePollResponse(bool success, const Json::Value& result)
			{
				if (!success || !mActive)
					return;

				mSnapshot.RemoveAll();
				ParseGetAllStatesResponse(result, mSnapshot);

				if (mState)
					mState->mSnapshotVersion++;

				PushSnapshotToUI();
			}

			void AssetStateTablePanel::PushSnapshotToUI()
			{
				if (!mBridge)
					return;

				Json::Value data;
				Json::Value assets(Json::arrayValue);

				for (unsigned int i = 0; i < mSnapshot.Size(); ++i)
				{
					const AssetStateRow& row = mSnapshot[i];
					Json::Value entry;
					entry["id"] = row.mAssetId.AsChar();
					entry["state"] = AssetStateEnumToString(row.mState);
					entry["scope"] = AssetScopeEnumToString(row.mScope);
					entry["refCount"] = static_cast<int>(row.mRefCount);
					entry["deployPath"] = row.mDeployPath.AsCStr();
					assets.append(entry);
				}

				data["assets"] = assets;
				data["total"] = static_cast<int>(mSnapshot.Size());
				mBridge->NotifyUIDataChanged("asset_runtime_editor.snapshot", data);
			}

			void AssetStateTablePanel::ParseGetAllStatesResponse(const Json::Value& data,
				Dia::Core::Containers::DynamicArrayC<AssetStateRow, kMaxAssets>& outRows)
			{
				if (!data.isMember("assets") || !data["assets"].isArray())
					return;

				const Json::Value& assets = data["assets"];
				for (unsigned int i = 0; i < assets.size() && !outRows.IsFull(); ++i)
				{
					const Json::Value& item = assets[i];
					AssetStateRow row;

					if (item.isMember("assetId") && item["assetId"].isString())
						row.mAssetId = Dia::Core::StringCRC(item["assetId"].asCString());
					else if (item.isMember("id") && item["id"].isString())
						row.mAssetId = Dia::Core::StringCRC(item["id"].asCString());

					if (item.isMember("state") && item["state"].isString())
						row.mState = StringToAssetStateEnum(item["state"].asCString());

					if (item.isMember("scope") && item["scope"].isString())
						row.mScope = StringToAssetScopeEnum(item["scope"].asCString());

					if (item.isMember("stageId") && item["stageId"].isString())
						row.mStageId = Dia::Core::StringCRC(item["stageId"].asCString());

					if (item.isMember("refCount") && item["refCount"].isNumeric())
						row.mRefCount = item["refCount"].asUInt();

					if (item.isMember("deployPath") && item["deployPath"].isString())
						row.mDeployPath = Dia::Core::Containers::String512(item["deployPath"].asCString());

					outRows.Add(row);
				}
			}
		}
	}
}
