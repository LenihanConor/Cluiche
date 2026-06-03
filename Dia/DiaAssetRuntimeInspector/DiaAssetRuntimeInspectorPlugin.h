#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Project/ProjectContext.h>
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
		class WebUIBridge;
		class EditorView;
		class IPluginLoader;
		class GameConnectionManager;
	}

	namespace AssetRuntime
	{
		namespace Inspector
		{
			struct SharedPluginState;

			class DiaAssetRuntimeInspectorPlugin : public Dia::Editor::IEditorPlugin
			{
			public:
				const char* GetName() const override { return "DiaAssetRuntimeInspector"; }
				const char* GetVersion() const override { return "1.0.0"; }
				const char* GetDescription() const override { return "Live asset runtime state inspector"; }
				const char* GetUIPath() const override { return "dia://plugins/assetruntimeinspector/index.html"; }
				Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }
				Dia::Editor::EditorToolbarItem GetToolbarItem() const override { Dia::Editor::EditorToolbarItem item = Dia::Editor::IEditorPlugin::GetToolbarItem(); item.pinned = true; return item; }

				void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
				void OnUnload() override;
				void OnUpdate(float deltaTime) override;

				SharedPluginState* GetPluginData();

			private:
				static void OnProjectChangedStatic(const Dia::Editor::ProjectContext& ctx, void* ud);
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
				char mExpectedDiagamePath[512] = {};
			};
		}
	}
}
