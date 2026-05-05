#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Command/CommandHistory.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/CatalogueManifestSerializer.h>
#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>

#include "DiaAssetCatalogueEditor/ManifestLoadHandler.h"
#include "DiaAssetCatalogueEditor/SessionContext.h"
#include "DiaAssetCatalogueEditor/Handlers/FileDiscoverer.h"
#include <DiaAssetCatalogue/ContentHasher.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class EditorView;
	}

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
				void RegisterRequestHandlers();
				void RegisterCRUDHandlers();
				void RegisterDiscovererHandlers();
				void RegisterRelationshipHandlers();
				void RegisterValidationHandlers();
				void PushDirtyState();
				void PushRegistryState();

				static Dia::AssetCatalogue::AssetRecord RecordFromJson(const Json::Value& data);
				static Json::Value RecordToJson(const Dia::AssetCatalogue::AssetRecord& rec);

				static const unsigned int kOutputDirLength = 512;
				char mOutputDir[kOutputDirLength];

				static const unsigned int kCurrentPathLength = 512;
				char mCurrentPath[kCurrentPathLength];

				Dia::AssetCatalogue::AssetRegistry             mRegistry;
				Dia::AssetCatalogue::CatalogueManifestSerializer mSerializer;
				Dia::AssetCatalogue::AssetTypeRegistry         mTypeRegistry;

				Dia::Editor::CommandHistory                    mHistory;
				ManifestLoadHandler                            mLoadHandler;
				SessionContext                                 mSessionContext;
				Dia::AssetCatalogue::ContentHasher             mContentHasher;
				FileDiscoverer                                 mFileDiscoverer;

				Dia::Editor::WebUIBridge*                      mBridge = nullptr;
				Dia::Editor::EditorView*                       mView   = nullptr;
			};
		}
	}
}
