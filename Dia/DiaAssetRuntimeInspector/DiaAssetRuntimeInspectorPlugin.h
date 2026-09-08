#pragma once

#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include <DiaCore/Json/external/json/json.h>

#include <memory>

#include "DiaAssetRuntimeInspector/Panels/AssetStateTablePanel.h"
#include "DiaAssetRuntimeInspector/Panels/StageAssetTreePanel.h"
#include "DiaAssetRuntimeInspector/Panels/RefCountInspectorPanel.h"
#include "DiaAssetRuntimeInspector/Panels/StateTransitionLogPanel.h"
#include "DiaAssetRuntimeInspector/SessionContext.h"

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Inspector
		{
			struct SharedPluginState;

			class DiaAssetRuntimeInspectorPlugin : public Dia::Editor::LiveConnectionPluginBase
			{
			public:
				DiaAssetRuntimeInspectorPlugin()
					: LiveConnectionPluginBase({
						"DiaAssetRuntimeInspector",
						"1.0.0",
						"Live asset runtime state inspector",
						"dia://plugins/assetruntimeinspector/dist/index.html",
						Dia::Editor::LayoutMode::kDockable,
						nullptr,
						nullptr,
						true
					}, "asset_runtime_inspector")
				{}

				SharedPluginState* GetPluginData();

			protected:
				void OnLivePluginLoad() override;
				void OnLivePluginUnload() override;
				void OnLiveUpdate(float deltaTime) override;
				void OnGameConnected() override;
				void OnGameDisconnected() override;
				void OnProjectChanged(const Dia::Editor::ProjectContext& ctx) override;

			private:
				void PushSavedFiltersToUI();
				void SaveCurrentFilters();

				std::unique_ptr<SharedPluginState> mState;

				AssetStateTablePanel mAssetStateTable;
				StageAssetTreePanel mStageAssetTree;
				RefCountInspectorPanel mRefCountInspector;
				StateTransitionLogPanel mTransitionLog;
				SessionContext mSessionContext;

				char mCurrentStateFilter[32] = {};
				char mCurrentIdSearch[128] = {};
				char mExpectedDiagamePath[512] = {};
			};
		}
	}
}
