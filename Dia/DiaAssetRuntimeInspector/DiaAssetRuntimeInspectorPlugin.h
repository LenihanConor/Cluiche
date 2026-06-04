#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaCore/Json/external/json/json.h>

#include <memory>

#include "DiaAssetRuntimeInspector/Panels/AssetStateTablePanel.h"
#include "DiaAssetRuntimeInspector/Panels/StageAssetTreePanel.h"
#include "DiaAssetRuntimeInspector/Panels/RefCountInspectorPanel.h"
#include "DiaAssetRuntimeInspector/Panels/StateTransitionLogPanel.h"
#include "DiaAssetRuntimeInspector/SessionContext.h"

namespace Dia
{
	namespace Editor
	{
		class GameConnectionManager;
	}

	namespace AssetRuntime
	{
		namespace Inspector
		{
			struct SharedPluginState;

			class DiaAssetRuntimeInspectorPlugin : public Dia::Editor::EditorPluginBase
			{
			public:
				DiaAssetRuntimeInspectorPlugin()
					: EditorPluginBase({
						"DiaAssetRuntimeInspector",
						"1.0.0",
						"Live asset runtime state inspector",
						"dia://plugins/assetruntimeinspector/index.html",
						Dia::Editor::LayoutMode::kDockable,
						nullptr,
						nullptr,
						true
					})
				{}

				SharedPluginState* GetPluginData();

			protected:
				void OnPluginLoad() override;
				void OnPluginUnload() override;
				void OnUpdate(float deltaTime) override;
				void OnProjectChanged(const Dia::Editor::ProjectContext& ctx) override;

			private:
				void HandleConnectionStateChange(bool connected);
				void PushSavedFiltersToUI();
				void SaveCurrentFilters();

				Dia::Editor::GameConnectionManager* mManager = nullptr;
				std::unique_ptr<SharedPluginState> mState;
				bool mWasConnected = false;

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
