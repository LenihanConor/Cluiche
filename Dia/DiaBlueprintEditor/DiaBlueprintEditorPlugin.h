#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaEditor/Project/ProjectContext.h>

#include "DiaBlueprintEditor/BlueprintFileHandler.h"
#include "DiaBlueprintEditor/BlueprintListController.h"
#include "DiaBlueprintEditor/BlueprintPropertyController.h"
#include "DiaBlueprintEditor/SchemaReader.h"

namespace Dia
{
	namespace BlueprintEditor
	{
		class DiaBlueprintEditorPlugin : public Dia::Editor::EditorPluginBase
		{
		public:
			DiaBlueprintEditorPlugin();

			void OnPluginLoad() override;
			void OnPluginUnload() override;
			void OnUpdate(float deltaTime) override;
			void OnNavigate(const Dia::Core::StringCRC& instanceId) override;

		protected:
			void OnProjectChanged(const Dia::Editor::ProjectContext& ctx) override;

		private:
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
		};
	}
}
