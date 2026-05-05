#pragma once

#include "DiaAssetRuntime/AssetState.h"
#include "DiaAssetRuntime/RuntimeManifestLoader.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String512.h"
#include "DiaCore/Containers/HashTables/HashTableC.h"
#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"

namespace Dia
{
    namespace AssetRuntime
    {
        class AssetRuntime
        {
        public:
            AssetRuntime();

            bool LoadManifest(const Dia::Core::FilePath& manifestPath);

            // Returns absolute deploy path for the asset, or null if not registered.
            const Dia::Core::Containers::String512* ResolveAssetPath(const Dia::Core::StringCRC& assetId) const;

            // State queries
            AssetState GetAssetState(const Dia::Core::StringCRC& assetId) const;
            bool IsAssetReady(const Dia::Core::StringCRC& assetId) const;

            // Consumer acknowledgements
            void AcknowledgeAssetLoaded(const Dia::Core::StringCRC& assetId);
            void AcknowledgeAssetUnloaded(const Dia::Core::StringCRC& assetId);

            // Stage lifecycle
            void RequestStageLoad(const Dia::Core::StringCRC& stageId);
            void RequestStageUnload(const Dia::Core::StringCRC& stageId);

            // Ref count query (debug)
            unsigned int GetAssetRefCount(const Dia::Core::StringCRC& assetId) const;

        private:
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

            void RegisterPathAliases();
            void InitStateTable();
            void InitRefCountTable();
            bool TryTransition(const Dia::Core::StringCRC& assetId, AssetState target);

            RuntimeManifestLoader::AssetTable mAssetTable;
            RuntimeManifestLoader::StageTable mStageTable;
            StateTable                        mStateTable;
            RefCountTable                     mRefCountTable;
        };
    }
}
