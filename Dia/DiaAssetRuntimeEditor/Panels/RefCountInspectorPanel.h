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

			class RefCountInspectorPanel
			{
			public:
				static const unsigned int kMaxStageReferences = 64;

				RefCountInspectorPanel();

				void Activate(Dia::Editor::WebUIBridge* bridge,
					Dia::Editor::GameConnectionManager* manager,
					SharedPluginState* state,
					const AssetStateTablePanel* tablePanel);
				void Deactivate();
				void Update(float deltaTime);

				void OnConnectionStateChanged(bool connected);

			private:
				void RefreshInspector();
				void PushInspectorToUI();
				void QueryStageDepsForAsset(const Dia::Core::StringCRC& assetId);

				struct StageReference
				{
					Dia::Core::StringCRC mStageId;
				};

				Dia::Editor::WebUIBridge* mBridge;
				Dia::Editor::GameConnectionManager* mManager;
				SharedPluginState* mState;
				const AssetStateTablePanel* mTablePanel;

				Dia::Core::StringCRC mLastInspectedAssetId;
				unsigned int mLastSnapshotVersion;

				Dia::Core::Containers::DynamicArrayC<StageReference, kMaxStageReferences> mStageRefs;

				bool mActive;
				bool mConnected;
			};
		}
	}
}
