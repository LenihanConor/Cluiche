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

			void RefCountInspectorPanel::QueryStageDepsForAsset(const Dia::Core::StringCRC& /*assetId*/)
			{
				// Stage reference resolution will be populated by get_stage_deps responses
				// For now, the ref count from the snapshot is the authoritative data.
				// A full implementation would query all loaded stages and check each deps list.
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
