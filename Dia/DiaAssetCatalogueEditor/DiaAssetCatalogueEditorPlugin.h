#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/CatalogueManifestSerializer.h>
#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			class DiaAssetCatalogueEditorPlugin : public Dia::Editor::IEditorPlugin
			{
			public:
				const char* GetName() const override { return "DiaAssetCatalogueEditor"; }
				const char* GetVersion() const override { return "1.0.0"; }
				const char* GetDescription() const override { return "Author and maintain the asset catalogue manifest"; }
				const char* GetUIPath() const override { return "dia://plugins/assetcatalogue/index.html"; }
				Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

				void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
				void OnUnload() override;
				void OnUpdate(float deltaTime) override;

			private:
				Dia::AssetCatalogue::AssetRegistry             mRegistry;
				Dia::AssetCatalogue::CatalogueManifestSerializer mSerializer;
				Dia::AssetCatalogue::AssetTypeRegistry         mTypeRegistry;
				Dia::AssetCatalogue::RelationshipIndex*        mRelationshipIndex = nullptr;
			};
		}
	}
}
