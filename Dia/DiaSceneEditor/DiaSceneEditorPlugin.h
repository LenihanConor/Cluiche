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

			// Testable handler methods — can be called directly with JSON fixtures
			Json::Value HandleAddItem(const Json::Value& data);
			Json::Value HandleDeleteItem(const Json::Value& data);
			Json::Value HandleDuplicateItem(const Json::Value& data);
			Json::Value HandleRenameItem(const Json::Value& data);
			Json::Value HandleLoadScene(const Json::Value& data);
			Json::Value HandleSaveScene(const Json::Value& data);
			Json::Value HandleValidate(const Json::Value& data);

			// Test support — inject a scene root without file I/O
			void SetTestScene(const Json::Value& sceneRoot)
			{
				mLoadedSceneRoot = sceneRoot;
				strncpy_s(mLoadedScenePath, sizeof(mLoadedScenePath), "test://scene.diascene", _TRUNCATE);
			}

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
