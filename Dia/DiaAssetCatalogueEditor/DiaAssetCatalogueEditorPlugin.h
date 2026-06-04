#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaEditor/Command/CommandHistory.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/CatalogueManifestSerializer.h>
#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>
#include <DiaAssetCatalogue/CatalogueRulesEngine.h>

#include "DiaAssetCatalogueEditor/ManifestLoadHandler.h"
#include "DiaAssetCatalogueEditor/SessionContext.h"
#include "DiaAssetCatalogueEditor/Handlers/FileDiscoverer.h"
#include "DiaAssetCatalogueEditor/Handlers/AssetTypeEditorRegistry.h"
#include <DiaAssetCatalogue/ContentHasher.h>

#include <unordered_map>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class EditorView;
		class IPluginLoader;
	}

	namespace AssetCatalogue
	{
		namespace Editor
		{
			class DiaAssetCatalogueEditorPlugin : public Dia::Editor::EditorPluginBase
			{
			public:
				DiaAssetCatalogueEditorPlugin();

				void OnPluginLoad() override;
				void OnPluginUnload() override;
				void OnUpdate(float deltaTime) override;
				void OnNavigate(const Dia::Core::StringCRC& instanceId) override;
				void OnProjectChanged(const Dia::Editor::ProjectContext& context) override;

			private:
				struct AssetTemplate
				{
					const char* content;
					const char* extension;
				};

				void RegisterRequestHandlers();
				void RegisterCRUDHandlers();
				void RegisterDiscovererHandlers();
				void RegisterRelationshipHandlers();
				void RegisterValidationHandlers();
				void RegisterAssetTypeEditorHandlers();
				void RegisterRulesHandlers();
				void RegisterInferrerHandlers();
				void SeedAssetTemplates();
				void PushDirtyState();
				void PushRegistryState();
				void AutoLoadRules();
				bool LoadManifestFromPath(const char* path, char* errorOut, unsigned int errorCapacity);
				void GetManifestDirectory(char* dirOut, unsigned int dirOutSize) const;
				void MakeRelativeToManifest(const char* absPath, char* relOut, unsigned int relOutSize) const;

				static Dia::AssetCatalogue::AssetRecord RecordFromJson(const Json::Value& data);
				static Json::Value RecordToJson(const Dia::AssetCatalogue::AssetRecord& rec);
				Json::Value RecordToJsonWithMeta(const Dia::AssetCatalogue::AssetRecord& rec) const;

				static const unsigned int kOutputDirLength = 512;
				char mOutputDir[kOutputDirLength];

				static const unsigned int kCurrentPathLength = 512;
				char mCurrentPath[kCurrentPathLength];

				static const unsigned int kDiagameDirLength = 512;
				char mDiagameDir[kDiagameDirLength];

				Dia::AssetCatalogue::AssetRegistry             mRegistry;
				Dia::AssetCatalogue::CatalogueManifestSerializer mSerializer;
				Dia::AssetCatalogue::AssetTypeRegistry         mTypeRegistry;

				Dia::Editor::CommandHistory                    mHistory;
				ManifestLoadHandler                            mLoadHandler;
				SessionContext                                 mSessionContext;
				Dia::AssetCatalogue::ContentHasher             mContentHasher;
				FileDiscoverer                                 mFileDiscoverer;

				AssetTypeEditorRegistry                        mTypeEditorRegistry;
				Dia::AssetCatalogue::CatalogueRulesEngine      mRulesEngine;

				std::unordered_map<Dia::Core::StringCRC, AssetTemplate> mAssetTemplates;
			};
		}
	}
}
