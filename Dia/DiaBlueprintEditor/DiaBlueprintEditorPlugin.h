#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaAssetCatalogue/AssetRegistry.h>

#include "DiaBlueprintEditor/BlueprintFileHandler.h"
#include "DiaBlueprintEditor/BlueprintListController.h"
#include "DiaBlueprintEditor/BlueprintPropertyController.h"

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class IPluginLoader;
	}

	namespace BlueprintEditor
	{
		class DiaBlueprintEditorPlugin : public Dia::Editor::IEditorPlugin
		{
		public:
			const char* GetName()        const override { return "DiaBlueprintEditor"; }
			const char* GetVersion()     const override { return "1.0.0"; }
			const char* GetDescription() const override { return "Author entity, camera, and light blueprint files"; }
			const char* GetUIPath()      const override { return "dia://plugins/blueprinteditor/index.html"; }
			Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

			void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;
			void OnNavigate(const Dia::Core::StringCRC& instanceId) override;

		private:
			static void OnProjectChangedStatic(const Dia::Editor::ProjectContext& ctx, void* ud);
			void RegisterRequestHandlers();
			void RegisterListHandlers();
			void RegisterPropertyHandlers();
			void RegisterFileHandlers();
			void RegisterAssetTypeHandlers();
			void RegisterAssetTypesWithCatalogue();

			BlueprintFileHandler       mFileHandler;
			BlueprintListController    mListController;
			BlueprintPropertyController mPropertyController;

			Dia::AssetCatalogue::AssetRegistry mRegistry;

			static const unsigned int kDiagamePathLength = 512;
			char mDiagamePath[kDiagamePathLength] = {};

			Dia::Editor::WebUIBridge*   mBridge       = nullptr;
			Dia::Editor::IPluginLoader* mPluginLoader = nullptr;
		};
	}
}
