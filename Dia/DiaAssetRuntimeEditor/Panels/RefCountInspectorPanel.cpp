#include "DiaAssetRuntimeEditor/Panels/RefCountInspectorPanel.h"
#include "DiaAssetRuntimeEditor/Panels/AssetStateTablePanel.h"
#include "DiaAssetRuntimeEditor/SharedPluginState.h"

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			RefCountInspectorPanel::RefCountInspectorPanel()
				: mBridge(nullptr)
				, mManager(nullptr)
				, mState(nullptr)
				, mTablePanel(nullptr)
				, mLastInspectedAssetId()
				, mLastSnapshotVersion(0)
				, mActive(false)
				, mConnected(false)
			{}

			void RefCountInspectorPanel::Activate(Dia::Editor::WebUIBridge* bridge,
				Dia::Editor::GameConnectionManager* manager,
				SharedPluginState* state,
				const AssetStateTablePanel* tablePanel)
			{
				mBridge = bridge;
				mManager = manager;
				mState = state;
				mTablePanel = tablePanel;
				mActive = true;
				mLastSnapshotVersion = 0;
			}

			void RefCountInspectorPanel::Deactivate()
			{
				mActive = false;
				mBridge = nullptr;
				mManager = nullptr;
				mState = nullptr;
				mTablePanel = nullptr;
			}

			void RefCountInspectorPanel::Update(float /*deltaTime*/)
			{
				if (!mActive || !mConnected || !mState || !mTablePanel)
					return;

				bool selectionChanged = (mState->mSelectedAssetId != mLastInspectedAssetId);
				bool snapshotChanged = (mState->mSnapshotVersion != mLastSnapshotVersion);

				if (selectionChanged || snapshotChanged)
				{
					mLastInspectedAssetId = mState->mSelectedAssetId;
					mLastSnapshotVersion = mState->mSnapshotVersion;
					RefreshInspector();
				}
			}

			void RefCountInspectorPanel::OnConnectionStateChanged(bool connected)
			{
				mConnected = connected;

				if (mBridge)
				{
					Json::Value data;
					data["connected"] = connected;
					mBridge->NotifyUIDataChanged("asset_runtime_editor.inspector_connection_state", data);
				}

				if (connected)
				{
					mLastSnapshotVersion = 0;
				}
				else
				{
					mStageRefs.RemoveAll();
					PushInspectorToUI();
				}
			}

			void RefCountInspectorPanel::RefreshInspector()
			{
				mStageRefs.RemoveAll();

				if (mLastInspectedAssetId == Dia::Core::StringCRC())
				{
					PushInspectorToUI();
					return;
				}

				// Find the asset in the snapshot
				const auto& snapshot = mTablePanel->GetSnapshot();
				const AssetStateRow* selectedRow = nullptr;

				for (unsigned int i = 0; i < snapshot.Size(); ++i)
				{
					if (snapshot[i].mAssetId == mLastInspectedAssetId)
					{
						selectedRow = &snapshot[i];
						break;
					}
				}

				if (!selectedRow)
				{
					PushInspectorToUI();
					return;
				}

				// For global assets, query each stage's deps to find references
				if (selectedRow->mScope == AssetScopeEnum::kGlobal && mManager)
				{
					QueryStageDepsForAsset(mLastInspectedAssetId);
				}

				PushInspectorToUI();
			}

			void RefCountInspectorPanel::QueryStageDepsForAsset(const Dia::Core::StringCRC& assetId)
			{
				if (!mManager || !mTablePanel)
					return;

				// Collect unique stage IDs from the snapshot
				const auto& snapshot = mTablePanel->GetSnapshot();
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> stages;

				for (unsigned int i = 0; i < snapshot.Size(); ++i)
				{
					const AssetStateRow& row = snapshot[i];
					if (row.mScope != AssetScopeEnum::kStage || row.mStageId == Dia::Core::StringCRC())
						continue;

					bool alreadyAdded = false;
					for (unsigned int s = 0; s < stages.Size(); ++s)
					{
						if (stages[s] == row.mStageId)
						{
							alreadyAdded = true;
							break;
						}
					}
					if (!alreadyAdded && !stages.IsFull())
						stages.Add(row.mStageId);
				}

				// Query each stage's deps to see if our asset appears
				Dia::Core::StringCRC capturedAssetId = assetId;
				for (unsigned int i = 0; i < stages.Size(); ++i)
				{
					Dia::Core::StringCRC stageId = stages[i];
					Json::Value args;
					args["stageId"] = stageId.AsChar();
					mManager->SendCommandWithResponse("asset_runtime.get_stage_deps", args,
						[this, capturedAssetId, stageId](bool success, const Json::Value& result)
						{
							if (!success || !mActive)
								return;

							if (result.isMember("assets") && result["assets"].isArray())
							{
								const Json::Value& assets = result["assets"];
								for (unsigned int j = 0; j < assets.size(); ++j)
								{
									if (assets[j].isString() &&
										Dia::Core::StringCRC(assets[j].asCString()) == capturedAssetId)
									{
										if (!mStageRefs.IsFull())
										{
											StageReference ref;
											ref.mStageId = stageId;
											mStageRefs.Add(ref);
										}
										PushInspectorToUI();
										break;
									}
								}
							}
						});
				}
			}

			void RefCountInspectorPanel::PushInspectorToUI()
			{
				if (!mBridge)
					return;

				Json::Value data;

				if (mLastInspectedAssetId == Dia::Core::StringCRC())
				{
					data["hasSelection"] = false;
					data["message"] = "Select an asset to inspect ref counts.";
					mBridge->NotifyUIDataChanged("asset_runtime_editor.inspector_data", data);
					return;
				}

				// Find asset in snapshot
				const auto& snapshot = mTablePanel->GetSnapshot();
				const AssetStateRow* row = nullptr;
				for (unsigned int i = 0; i < snapshot.Size(); ++i)
				{
					if (snapshot[i].mAssetId == mLastInspectedAssetId)
					{
						row = &snapshot[i];
						break;
					}
				}

				if (!row)
				{
					data["hasSelection"] = true;
					data["missing"] = true;
					data["message"] = "Asset no longer present in runtime.";
					data["assetId"] = mLastInspectedAssetId.AsChar();
					mBridge->NotifyUIDataChanged("asset_runtime_editor.inspector_data", data);
					return;
				}

				data["hasSelection"] = true;
				data["missing"] = false;
				data["assetId"] = row->mAssetId.AsChar();
				data["state"] = AssetStateEnumToString(row->mState);
				data["scope"] = AssetScopeEnumToString(row->mScope);
				data["refCount"] = static_cast<int>(row->mRefCount);

				if (row->mScope == AssetScopeEnum::kStage)
				{
					data["stageScoped"] = true;
					data["message"] = "Stage-scoped asset -- single reference from owning stage.";
				}
				else
				{
					data["stageScoped"] = false;
					Json::Value refs(Json::arrayValue);
					for (unsigned int i = 0; i < mStageRefs.Size(); ++i)
					{
						Json::Value ref;
						ref["stageId"] = mStageRefs[i].mStageId.AsChar();
						refs.append(ref);
					}
					data["stageRefs"] = refs;
				}

				mBridge->NotifyUIDataChanged("asset_runtime_editor.inspector_data", data);
			}
		}
	}
}
