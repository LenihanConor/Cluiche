#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaEditor/Project/ProjectContext.h>

#include "DiaEntityTemplateEditor/BlueprintFileHandler.h"
#include "DiaEntityTemplateEditor/BlueprintListController.h"
#include "DiaEntityTemplateEditor/BlueprintPropertyController.h"
#include "DiaEntityTemplateEditor/SchemaReader.h"

namespace Dia
{
	namespace EntityTemplateEditor
	{
		class DiaEntityTemplateEditorPlugin : public Dia::Editor::EditorPluginBase
		{
		public:
			DiaEntityTemplateEditorPlugin();

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
			void DualRegisterActions();

			// Query the shared catalogue registry for blueprint assets of a given type.
			// Returns the "records" array from asset_catalogue.query_by_type, or empty array on failure.
			Json::Value QueryCatalogueByType(const char* typeId) const;

			// Resolve a relative source_path to an absolute path using mDiagameDir.
			void ResolvePath(const char* relPath, char* absOut, unsigned int absCapacity) const;

			BlueprintFileHandler        mFileHandler;
			BlueprintListController     mListController;
			BlueprintPropertyController mPropertyController;
			SchemaReader                mSchemaReader;

			static const unsigned int kDiagamePathLength = 512;
			static const unsigned int kDiagameDirLength  = 512;
			char mDiagamePath[kDiagamePathLength] = {};
			char mDiagameDir[kDiagameDirLength]   = {};
		};
	}
}
