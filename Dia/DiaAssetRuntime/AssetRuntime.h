#pragma once

#include "DiaAssetRuntime/AssetScope.h"
#include "DiaAssetRuntime/AssetState.h"
#include "DiaAssetRuntime/IAssetTypeHandler.h"
#include "DiaAssetRuntime/RuntimeManifestLoader.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String512.h"
#include "DiaCore/Containers/HashTables/HashTableC.h"
#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
    namespace AssetRuntime
    {
        class AssetRuntime : public IAssetLoadCallback
        {
        public:
            struct LoadProgress
            {
                unsigned int loaded;
                unsigned int total;
                unsigned int failed;
            };

            AssetRuntime();

            bool LoadManifest(const Dia::Core::FilePath& manifestPath);
            bool LoadManifest(const Dia::Core::FilePath::ResoledFilePath& resolvedManifestPath);

            const Dia::Core::Containers::String512* ResolveAssetPath(const Dia::Core::StringCRC& assetId) const;

            // State queries
            AssetState GetAssetState(const Dia::Core::StringCRC& assetId) const;
            bool IsAssetReady(const Dia::Core::StringCRC& assetId) const;
            bool IsLoadComplete(const Dia::Core::StringCRC& stageId) const;

            // Stage lifecycle
            void RequestStageLoad(const Dia::Core::StringCRC& stageId);
            void RequestStageUnload(const Dia::Core::StringCRC& stageId);

            // Type handler registration
            void RegisterTypeHandler(const char* typePrefix, IAssetTypeHandler* handler);
            void UnregisterTypeHandler(const char* typePrefix);

            // Explicit retry for failed assets
            void RetryAssetLoad(const Dia::Core::StringCRC& assetId);

            // Load progress for a stage
            LoadProgress GetLoadProgress(const Dia::Core::StringCRC& stageId) const;

            // Ref count query (debug)
            unsigned int GetAssetRefCount(const Dia::Core::StringCRC& assetId) const;

            // Scope query (debug)
            AssetScope GetAssetScope(const Dia::Core::StringCRC& assetId) const;

            // Returns the stage ID that owns this asset (empty StringCRC if global or not found)
            Dia::Core::StringCRC GetAssetStageId(const Dia::Core::StringCRC& assetId) const;

            // Teardown
            void Reset();

            // Debug queries
            unsigned int GetAllAssets(
                Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const;
            unsigned int GetLoadedAssets(
                Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const;
            unsigned int GetStagedAssets(
                Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const;
            unsigned int GetStageDependencies(
                const Dia::Core::StringCRC& stageId,
                Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 128>& results) const;

        private:
            // IAssetLoadCallback
            virtual void OnLoadComplete(const Dia::Core::StringCRC& assetId) override;
            virtual void OnLoadFailed(const Dia::Core::StringCRC& assetId, const char* reason) override;

            class StateHashFunctor
            {
            public:
                typedef Dia::Core::StringCRC Key;
                typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

                unsigned int GetHashIndex(const Key& key, const TableData* tableData) const;
            };

            static const unsigned int kMaxAssets      = RuntimeManifestLoader::kMaxAssets;
            static const unsigned int kStateTableSize  = RuntimeManifestLoader::kAssetTableSize;
            static const unsigned int kRefTableSize    = RuntimeManifestLoader::kAssetTableSize;
            static const unsigned int kMaxHandlers     = 16;

            typedef Dia::Core::Containers::HashTableC<
                Dia::Core::StringCRC,
                AssetState,
                StateHashFunctor,
                kMaxAssets,
                kStateTableSize> StateTable;

            typedef Dia::Core::Containers::HashTableC<
                Dia::Core::StringCRC,
                unsigned int,
                StateHashFunctor,
                kMaxAssets,
                kRefTableSize> RefCountTable;

            struct HandlerEntry
            {
                Dia::Core::StringCRC mTypePrefixCRC;
                Dia::Core::Containers::String32 mTypePrefix;
                IAssetTypeHandler* mHandler;
            };

            void RegisterPathAliases();
            void InitStateTable();
            void InitRefCountTable();
            bool TryTransition(const Dia::Core::StringCRC& assetId, AssetState target);
            void DispatchLoad(const Dia::Core::StringCRC& assetId);
            void AutoValidate(const Dia::Core::StringCRC& assetId,
                              const Dia::Core::Containers::String512& resolvedPath);
            void DispatchUnload(const Dia::Core::StringCRC& assetId);
            Dia::Core::StringCRC ExtractTypePrefix(const Dia::Core::StringCRC& assetId) const;
            IAssetTypeHandler* FindHandler(const Dia::Core::StringCRC& typePrefixCRC) const;
            void AssertOwnerThread() const;

            RuntimeManifestLoader::AssetTable mAssetTable;
            RuntimeManifestLoader::StageTable mStageTable;
            StateTable                        mStateTable;
            RefCountTable                     mRefCountTable;

            Dia::Core::Containers::DynamicArrayC<HandlerEntry, kMaxHandlers> mHandlers;
            unsigned int                      mOwnerThreadId;
        };
    }
}
