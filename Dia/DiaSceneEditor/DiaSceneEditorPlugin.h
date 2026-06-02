#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaAssetCatalogue/AssetRegistry.h>

#include "DiaSceneEditor/SceneFileHandler.h"
#include "DiaSceneEditor/SceneHierarchyController.h"
#include "DiaSceneEditor/PropertyInspectorController.h"

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class IPluginLoader;
	}

	namespace SceneEditor
	{
		class DiaSceneEditorPlugin : public Dia::Editor::IEditorPlugin
		{
		public:
			const char* GetName()        const override { return "DiaSceneEditor"; }
			const char* GetVersion()     const override { return "1.0.0"; }
			const char* GetDescription() const override { return "Author scene files and entity placements"; }
			const char* GetUIPath()      const override { return "dia://plugins/sceneeditor/index.html"; }
			Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

			void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;

		private:
			static void OnProjectChangedStatic(const Dia::Editor::ProjectContext& ctx, void* ud);
			void RegisterRequestHandlers();

			SceneFileHandler           mFileHandler;
			SceneHierarchyController   mHierarchyController;
			PropertyInspectorController mPropertyController;

			Dia::Editor::WebUIBridge*   mBridge       = nullptr;
			Dia::Editor::IPluginLoader* mPluginLoader = nullptr;
		};
	}
}
