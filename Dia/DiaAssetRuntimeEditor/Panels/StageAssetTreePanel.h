#pragma once

#include "DiaAssetRuntimeEditor/Panels/AssetStateRow.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class GameConnectionManager;
	}

	namespace AssetRuntime
	{
		namespace Editor
		{
			struct SharedPluginState;
			class AssetStateTablePanel;

			class StageAssetTreePanel
			{
			public:
				static const unsigned int kMaxStages = 128;
				static const unsigned int kMaxAssetsPerStage = 512;

				StageAssetTreePanel();

				void Activate(Dia::Editor::WebUIBridge* bridge,
					Dia::Editor::GameConnectionManager* manager,
					SharedPluginState* state,
					const AssetStateTablePanel* tablePanel);
				void Deactivate();
				void Update(float deltaTime);

				void OnConnectionStateChanged(bool connected);

			private:
				struct StageNode
				{
					Dia::Core::StringCRC mStageId;
					unsigned int mAssetCount;
					bool mExpanded;
					bool mChildrenLoaded;
					unsigned int mLastSnapshotVersion;

					StageNode()
						: mStageId()
						, mAssetCount(0)
						, mExpanded(false)
						, mChildrenLoaded(false)
						, mLastSnapshotVersion(0)
					{}
				};

				void RebuildRootNodes();
				void PushTreeToUI();
				void HandleExpandRequest(const Dia::Core::StringCRC& stageId);
				void HandleStageDepsResponse(const Dia::Core::StringCRC& stageId, bool success, const Json::Value& result);

				Dia::Editor::WebUIBridge* mBridge;
				Dia::Editor::GameConnectionManager* mManager;
				SharedPluginState* mState;
				const AssetStateTablePanel* mTablePanel;

				Dia::Core::Containers::DynamicArrayC<StageNode, kMaxStages> mStageNodes;

				unsigned int mLastProcessedSnapshotVersion;
				unsigned int mGeneration;
				bool mActive;
				bool mConnected;
			};
		}
	}
}
