#include "DiaAssetRuntimeEditor/Panels/StageAssetTreePanel.h"
#include "DiaAssetRuntimeEditor/Panels/AssetStateTablePanel.h"
#include "DiaAssetRuntimeEditor/SharedPluginState.h"

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaLogger/DiaLog.h>

#include <cstring>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			StageAssetTreePanel::StageAssetTreePanel()
				: mBridge(nullptr)
				, mManager(nullptr)
				, mState(nullptr)
				, mTablePanel(nullptr)
				, mLastProcessedSnapshotVersion(0)
				, mGeneration(0)
				, mActive(false)
				, mConnected(false)
			{}

			void StageAssetTreePanel::Activate(Dia::Editor::WebUIBridge* bridge,
				Dia::Editor::GameConnectionManager* manager,
				SharedPluginState* state,
				const AssetStateTablePanel* tablePanel)
			{
				mBridge = bridge;
				mManager = manager;
				mState = state;
				mTablePanel = tablePanel;
				mActive = true;
				mLastProcessedSnapshotVersion = 0;

				if (mBridge)
				{
					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.expand_stage"),
						[this](const Json::Value& data) -> Json::Value
						{
							Json::Value result;
							if (data.isMember("stageId") && data["stageId"].isString())
							{
								HandleExpandRequest(Dia::Core::StringCRC(data["stageId"].asCString()));
								result["success"] = true;
							}
							else
							{
								result["success"] = false;
								result["error"] = "missing stageId";
							}
							return result;
						});

					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.collapse_stage"),
						[this](const Json::Value& data) -> Json::Value
						{
							Json::Value result;
							if (data.isMember("stageId") && data["stageId"].isString())
							{
								Dia::Core::StringCRC stageId(data["stageId"].asCString());
								for (unsigned int i = 0; i < mStageNodes.Size(); ++i)
								{
									if (mStageNodes[i].mStageId == stageId)
									{
										mStageNodes[i].mExpanded = false;
										break;
									}
								}
								result["success"] = true;
							}
							else
							{
								result["success"] = false;
								result["error"] = "missing stageId";
							}
							return result;
						});

					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.tree_select_asset"),
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

			void StageAssetTreePanel::Deactivate()
			{
				++mGeneration;

				if (mBridge)
				{
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.expand_stage"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.collapse_stage"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.tree_select_asset"));
				}

				mActive = false;
				mBridge = nullptr;
				mManager = nullptr;
				mState = nullptr;
				mTablePanel = nullptr;
			}

			void StageAssetTreePanel::Update(float /*deltaTime*/)
			{
				if (!mActive || !mConnected || !mState || !mTablePanel)
					return;

				if (mState->mSnapshotVersion != mLastProcessedSnapshotVersion)
				{
					mLastProcessedSnapshotVersion = mState->mSnapshotVersion;
					RebuildRootNodes();
					PushTreeToUI();
				}
			}

			void StageAssetTreePanel::OnConnectionStateChanged(bool connected)
			{
				mConnected = connected;

				if (mBridge)
				{
					Json::Value data;
					data["connected"] = connected;
					mBridge->NotifyUIDataChanged("asset_runtime_editor.tree_connection_state", data);
				}

				if (!connected)
				{
					mStageNodes.RemoveAll();
					mLastProcessedSnapshotVersion = 0;
					PushTreeToUI();
				}
			}

			void StageAssetTreePanel::RebuildRootNodes()
			{
				if (!mTablePanel)
					return;

				const auto& snapshot = mTablePanel->GetSnapshot();

				// Collect unique stage IDs and count global assets
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxStages> stageIds;
				unsigned int globalCount = 0;

				for (unsigned int i = 0; i < snapshot.Size(); ++i)
				{
					const AssetStateRow& row = snapshot[i];
					if (row.mScope == AssetScopeEnum::kGlobal)
					{
						globalCount++;
					}
					else
					{
						// Extract stage from deploy path or use asset state info
						// For stage-scoped assets, we derive stage from the snapshot data
						// The stage ID is embedded in the data — look for unique identifiers
						// For now, we count stage-scoped assets. Real stage IDs come from get_stage_deps.
					}
				}

				// Preserve expand state: build a lookup of currently expanded nodes
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxStages> expandedStages;
				for (unsigned int i = 0; i < mStageNodes.Size(); ++i)
				{
					if (mStageNodes[i].mExpanded)
						expandedStages.Add(mStageNodes[i].mStageId);
				}

				mStageNodes.RemoveAll();

				// Add [Global] node
				if (globalCount > 0)
				{
					StageNode globalNode;
					globalNode.mStageId = Dia::Core::StringCRC("[Global]");
					globalNode.mAssetCount = globalCount;
					globalNode.mChildrenLoaded = true; // global assets are in the snapshot already
					for (unsigned int e = 0; e < expandedStages.Size(); ++e)
					{
						if (expandedStages[e] == globalNode.mStageId)
						{
							globalNode.mExpanded = true;
							break;
						}
					}
					mStageNodes.Add(globalNode);
				}

				// Derive stage nodes from stage-scoped assets
				for (unsigned int i = 0; i < snapshot.Size(); ++i)
				{
					const AssetStateRow& row = snapshot[i];
					if (row.mScope != AssetScopeEnum::kStage)
						continue;

					if (row.mStageId == Dia::Core::StringCRC())
						continue;

					// Check if this stage already has a node
					bool found = false;
					for (unsigned int s = 0; s < mStageNodes.Size(); ++s)
					{
						if (mStageNodes[s].mStageId == row.mStageId)
						{
							mStageNodes[s].mAssetCount++;
							found = true;
							break;
						}
					}

					if (!found && !mStageNodes.IsFull())
					{
						StageNode node;
						node.mStageId = row.mStageId;
						node.mAssetCount = 1;
						node.mLastSnapshotVersion = mLastProcessedSnapshotVersion;
						for (unsigned int e = 0; e < expandedStages.Size(); ++e)
						{
							if (expandedStages[e] == node.mStageId)
							{
								node.mExpanded = true;
								break;
							}
						}
						mStageNodes.Add(node);
					}
				}
			}

			void StageAssetTreePanel::PushTreeToUI()
			{
				if (!mBridge)
					return;

				Json::Value data;
				Json::Value nodes(Json::arrayValue);

				for (unsigned int i = 0; i < mStageNodes.Size(); ++i)
				{
					const StageNode& node = mStageNodes[i];
					Json::Value entry;
					entry["stageId"] = node.mStageId.AsChar();
					entry["assetCount"] = static_cast<int>(node.mAssetCount);
					entry["expanded"] = node.mExpanded;
					entry["childrenLoaded"] = node.mChildrenLoaded;
					nodes.append(entry);
				}

				// Include global asset list for the [Global] node
				if (mTablePanel)
				{
					const auto& snapshot = mTablePanel->GetSnapshot();
					Json::Value globalAssets(Json::arrayValue);
					for (unsigned int i = 0; i < snapshot.Size(); ++i)
					{
						const AssetStateRow& row = snapshot[i];
						if (row.mScope == AssetScopeEnum::kGlobal)
						{
							Json::Value asset;
							asset["id"] = row.mAssetId.AsChar();
							asset["state"] = AssetStateEnumToString(row.mState);
							asset["refCount"] = static_cast<int>(row.mRefCount);
							globalAssets.append(asset);
						}
					}
					data["globalAssets"] = globalAssets;
				}

				data["stages"] = nodes;
				if (mState)
					data["selectedAssetId"] = mState->mSelectedAssetId.AsChar();

				mBridge->NotifyUIDataChanged("asset_runtime_editor.tree_data", data);
			}

			void StageAssetTreePanel::HandleExpandRequest(const Dia::Core::StringCRC& stageId)
			{
				for (unsigned int i = 0; i < mStageNodes.Size(); ++i)
				{
					if (mStageNodes[i].mStageId == stageId)
					{
						mStageNodes[i].mExpanded = true;

						if (!mStageNodes[i].mChildrenLoaded ||
							mStageNodes[i].mLastSnapshotVersion != mLastProcessedSnapshotVersion)
						{
							// Fetch children from game
							if (mManager)
							{
								Json::Value args;
								args["stageId"] = stageId.AsChar();
								Dia::Core::StringCRC capturedId = stageId;
								unsigned int gen = mGeneration;
								mManager->SendCommandWithResponse("asset-runtime-get-stage-deps", args,
									[this, capturedId, gen](bool success, const Json::Value& result)
									{
										if (gen != mGeneration)
											return;
										HandleStageDepsResponse(capturedId, success, result);
									});
							}
						}
						else
						{
							PushTreeToUI();
						}
						break;
					}
				}
			}

			void StageAssetTreePanel::HandleStageDepsResponse(const Dia::Core::StringCRC& stageId,
				bool success, const Json::Value& result)
			{
				if (!success || !mActive)
					return;

				for (unsigned int i = 0; i < mStageNodes.Size(); ++i)
				{
					if (mStageNodes[i].mStageId == stageId)
					{
						mStageNodes[i].mChildrenLoaded = true;
						mStageNodes[i].mLastSnapshotVersion = mLastProcessedSnapshotVersion;

						if (result.isMember("assets") && result["assets"].isArray())
							mStageNodes[i].mAssetCount = result["assets"].size();

						break;
					}
				}

				// Push expanded children to UI
				if (mBridge)
				{
					Json::Value data;
					data["stageId"] = stageId.AsChar();
					data["assets"] = result.isMember("assets") ? result["assets"] : Json::Value(Json::arrayValue);
					mBridge->NotifyUIDataChanged("asset_runtime_editor.stage_children", data);
				}

				PushTreeToUI();
			}
		}
	}
}
