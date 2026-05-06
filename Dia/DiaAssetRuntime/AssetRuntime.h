#pragma once

#include "DiaAssetRuntime/AssetScope.h"
#include "DiaAssetRuntime/AssetState.h"
#include "DiaAssetRuntime/IAssetStateListener.h"
#include "DiaAssetRuntime/RuntimeManifestLoader.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String512.h"
#include "DiaCore/Containers/HashTables/HashTableC.h"
#include "DiaCore/Containers/HashTables/HashTableHashFunctionData.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

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
            void AcknowledgeAssetLoadFailed(const Dia::Core::StringCRC& assetId);

            // Stage lifecycle
            void RequestStageLoad(const Dia::Core::StringCRC& stageId);
            void RequestStageUnload(const Dia::Core::StringCRC& stageId);

            // Ref count query (debug)
            unsigned int GetAssetRefCount(const Dia::Core::StringCRC& assetId) const;

            // Scope query (debug)
            AssetScope GetAssetScope(const Dia::Core::StringCRC& assetId) const;

            // Returns the stage ID that owns this asset (empty StringCRC if global or not found)
            Dia::Core::StringCRC GetAssetStageId(const Dia::Core::StringCRC& assetId) const;

            // Listener registration
            void RegisterListener(IAssetStateListener* listener);
            void UnregisterListener(IAssetStateListener* listener);

            // Teardown — fire OnAssetUnloading for all non-Registered assets, reset all state/ref-counts
            void Reset();

            // Debug queries (const, read-only)
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
            void DispatchAssetReady(const Dia::Core::StringCRC& assetId);
            void DispatchAssetUnloading(const Dia::Core::StringCRC& assetId);
            void DispatchAssetLoadFailed(const Dia::Core::StringCRC& assetId);
            void FlushDeferredRemovals();
            void AssertOwnerThread() const;

            static const unsigned int kMaxListeners = 16;

            RuntimeManifestLoader::AssetTable mAssetTable;
            RuntimeManifestLoader::StageTable mStageTable;
            StateTable                        mStateTable;
            RefCountTable                     mRefCountTable;

            Dia::Core::Containers::DynamicArrayC<IAssetStateListener*, kMaxListeners> mListeners;
            Dia::Core::Containers::DynamicArrayC<IAssetStateListener*, kMaxListeners> mDeferredRemovals;
            bool                              mIsDispatching;
            unsigned int                      mOwnerThreadId;
        };
    }
}
