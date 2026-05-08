#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/Json/external/json/json.h>

#include <memory>

#include "DiaAssetRuntimeEditor/Panels/AssetStateTablePanel.h"
#include "DiaAssetRuntimeEditor/Panels/StageAssetTreePanel.h"
#include "DiaAssetRuntimeEditor/Panels/RefCountInspectorPanel.h"
#include "DiaAssetRuntimeEditor/Panels/StateTransitionLogPanel.h"
#include "DiaAssetRuntimeEditor/SessionContext.h"

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class EditorView;
		class IPluginLoader;
		class GameConnectionManager;
	}

	namespace AssetRuntime
	{
		namespace Editor
		{
			struct SharedPluginState;

			class DiaAssetRuntimeEditorPlugin : public Dia::Editor::IEditorPlugin
			{
			public:
				const char* GetName() const override { return "DiaAssetRuntimeEditor"; }
				const char* GetVersion() const override { return "1.0.0"; }
				const char* GetDescription() const override { return "Live asset runtime state inspector"; }
				const char* GetUIPath() const override { return "dia://plugins/assetruntimeeditor/index.html"; }
				Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

				void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
				void OnUnload() override;
				void OnUpdate(float deltaTime) override;

				SharedPluginState* GetPluginData();

			private:
				void RegisterRequestHandlers();
				void HandleConnectionStateChange(bool connected);
				void PushSavedFiltersToUI();
				void SaveCurrentFilters();

				Dia::Editor::WebUIBridge* mBridge = nullptr;
				Dia::Editor::EditorView* mView = nullptr;
				Dia::Editor::IPluginLoader* mPluginLoader = nullptr;

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
			};
		}
	}
}
