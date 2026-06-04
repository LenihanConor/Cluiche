#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
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
	namespace SceneEditor
	{
		class DiaSceneEditorPlugin : public Dia::Editor::EditorPluginBase
		{
		public:
			DiaSceneEditorPlugin()
				: EditorPluginBase({
					"DiaSceneEditor",
					"1.0.0",
					"Author scene files and entity placements",
					"dia://plugins/sceneeditor/index.html",
					Dia::Editor::LayoutMode::kDockable,
					"scene_editor.dirty_changed",
					nullptr,
					false
				})
			{
				mLoadedScenePath[0] = '\0';
			}

			void OnNavigate(const Dia::Core::StringCRC& instanceId) override;

		protected:
			void OnPluginLoad() override;
			void OnPluginUnload() override;
			void OnProjectChanged(const Dia::Editor::ProjectContext& ctx) override;

		private:
			void RegisterRequestHandlers();
			void ResolveCatalogueIdForLoadedScene();

			SceneFileHandler            mFileHandler;
			SceneValidator              mValidator;
			SceneHierarchyController    mHierarchyController;
			PropertyInspectorController mPropertyController;
			ProjectContextManager       mProjectContextManager;

			Json::Value                 mStageList;
			Json::Value                 mLoadedSceneRoot;
			char                        mLoadedScenePath[512];
			char                        mDiagamePath[512] = {};
			char                        mSceneCatalogueId[256] = {};
		};
	}
}
