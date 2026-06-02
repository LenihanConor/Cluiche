#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaCore/Json/external/json/json.h>

#include "DiaSceneEditor/SceneFileHandler.h"
#include "DiaSceneEditor/SceneHierarchyController.h"
#include "DiaSceneEditor/PropertyInspectorController.h"
#include "DiaSceneEditor/ProjectContextManager.h"
#include "DiaSceneEditor/SceneMutator.h"
#include "DiaSceneEditor/SceneValidator.h"

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

			SceneFileHandler            mFileHandler;
			SceneValidator              mValidator;
			SceneHierarchyController    mHierarchyController;
			PropertyInspectorController mPropertyController;
			ProjectContextManager       mProjectContextManager;

			Json::Value                 mStageList;             // cached from last project load
			Json::Value                 mLoadedSceneRoot;       // last successfully loaded .diascene
			char                        mLoadedScenePath[512];  // path for the loaded scene
			bool                        mIsDirty = false;       // unsaved edits exist

			Dia::Editor::WebUIBridge*   mBridge       = nullptr;
			Dia::Editor::IPluginLoader* mPluginLoader = nullptr;
		};
	}
}
