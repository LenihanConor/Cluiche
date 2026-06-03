#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Project/ProjectContext.h>

#include "DiaBlueprintEditor/BlueprintFileHandler.h"
#include "DiaBlueprintEditor/BlueprintListController.h"
#include "DiaBlueprintEditor/BlueprintPropertyController.h"
#include "DiaBlueprintEditor/SchemaReader.h"

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
			void RegisterAssetTypesWithCatalogue();

			// Query the shared catalogue registry for blueprint assets of a given type.
			// Returns the "records" array from asset_catalogue.query_by_type, or empty array on failure.
			Json::Value QueryCatalogueByType(const char* typeId) const;

			BlueprintFileHandler        mFileHandler;
			BlueprintListController     mListController;
			BlueprintPropertyController mPropertyController;
			SchemaReader                mSchemaReader;

			static const unsigned int kDiagamePathLength = 512;
			char mDiagamePath[kDiagamePathLength] = {};

			Dia::Editor::WebUIBridge*   mBridge       = nullptr;
			Dia::Editor::IPluginLoader* mPluginLoader = nullptr;
		};
	}
}
