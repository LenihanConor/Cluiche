#include "DiaAssetRuntime/AssetRuntime.h"

#include "DiaCore/FilePath/PathStore.h"
#include "DiaCore/FilePath/Path.h"
#include "DiaCore/CRC/CRC.h"

#include <DiaLogger/DiaLog.h>

#include <math.h>

namespace Dia
{
    namespace AssetRuntime
    {
        //------------------------------------------------------------------------------------
        // StateHashFunctor
        //------------------------------------------------------------------------------------
        unsigned int AssetRuntime::StateHashFunctor::GetHashIndex(const Key& key, const TableData* tableData) const
        {
            static const unsigned int sTranslationToTableSpace =
                Dia::Core::CRC::MaxCRC() / AssetRuntime::kStateTableSize;
            DIA_ASSERT(key.Value() != 0, "Cannot hash a zero CRC key");
            return static_cast<unsigned int>(floorf(static_cast<float>(key.Value()) /
                                                    static_cast<float>(sTranslationToTableSpace)));
        }

        //------------------------------------------------------------------------------------
        // AssetRuntime
        //------------------------------------------------------------------------------------
        AssetRuntime::AssetRuntime()
        {}

        bool AssetRuntime::LoadManifest(const Dia::Core::FilePath& manifestPath)
        {
            RuntimeManifestLoader loader;
            if (!loader.Load(manifestPath, mAssetTable, mStageTable))
                return false;

            InitStateTable();
            RegisterPathAliases();
            return true;
        }

        const Dia::Core::Containers::String512* AssetRuntime::ResolveAssetPath(const Dia::Core::StringCRC& assetId) const
        {
            const RuntimeAssetEntry* entry = mAssetTable.TryGetItemConst(assetId);
            if (!entry)
            {
                DIA_LOG_WARNING("AssetRuntime", "ResolveAssetPath: unknown asset '%s'", assetId.AsChar());
                return nullptr;
            }
            return &entry->mDeployPath;
        }

        AssetState AssetRuntime::GetAssetState(const Dia::Core::StringCRC& assetId) const
        {
            const AssetState* state = mStateTable.TryGetItemConst(assetId);
            if (!state)
            {
                DIA_LOG_WARNING("AssetRuntime", "GetAssetState: unknown asset '%s'", assetId.AsChar());
                return AssetState::Registered;
            }
            return *state;
        }

        bool AssetRuntime::IsAssetReady(const Dia::Core::StringCRC& assetId) const
        {
            const AssetState* state = mStateTable.TryGetItemConst(assetId);
            if (!state)
            {
                DIA_LOG_WARNING("AssetRuntime", "IsAssetReady: unknown asset '%s'", assetId.AsChar());
                return false;
            }
            return *state == AssetState::Loaded;
        }

        void AssetRuntime::AcknowledgeAssetLoaded(const Dia::Core::StringCRC& assetId)
        {
            TryTransition(assetId, AssetState::Loaded);
        }

        void AssetRuntime::AcknowledgeAssetUnloaded(const Dia::Core::StringCRC& assetId)
        {
            TryTransition(assetId, AssetState::Registered);
        }

        //------------------------------------------------------------------------------------
        // Private
        //------------------------------------------------------------------------------------
        void AssetRuntime::InitStateTable()
        {
            // Initialize all loaded assets to Registered state.
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                mStateTable.Add(entry.mId, AssetState::Registered);
            }
        }

        void AssetRuntime::RegisterPathAliases()
        {
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                const Dia::Core::StringCRC& id = entry.mId;
                const char* pathStr = entry.mDeployPath.AsCStr();

                if (Dia::Core::PathStore::IsPathAliasRegistered(id))
                {
                    DIA_LOG_WARNING("AssetRuntime", "RegisterPathAliases: alias '%s' already registered — skipping", id.AsChar());
                    continue;
                }

                Dia::Core::Path::String pathValue(pathStr);
                Dia::Core::PathStore::RegisterToStore(id, pathValue);
            }
        }

        bool AssetRuntime::TryTransition(const Dia::Core::StringCRC& assetId, AssetState target)
        {
            AssetState* state = mStateTable.TryGetItem(assetId);
            if (!state)
            {
                DIA_LOG_WARNING("AssetRuntime", "TryTransition: unknown asset '%s'", assetId.AsChar());
                return false;
            }

            AssetState current = *state;

            // Validate allowed transitions
            bool valid = false;
            switch (current)
            {
                case AssetState::Registered: valid = (target == AssetState::Staged);    break;
                case AssetState::Staged:     valid = (target == AssetState::Loaded)
                                                  || (target == AssetState::Unloading); break;
                case AssetState::Loaded:     valid = (target == AssetState::Unloading); break;
                case AssetState::Unloading:  valid = (target == AssetState::Registered); break;
            }

            if (!valid)
            {
                DIA_LOG_WARNING("AssetRuntime",
                    "TryTransition: invalid transition for '%s': %d -> %d",
                    assetId.AsChar(),
                    static_cast<int>(current),
                    static_cast<int>(target));
                return false;
            }

            DIA_LOG_DEBUG("AssetRuntime",
                "TryTransition: '%s' %d -> %d",
                assetId.AsChar(),
                static_cast<int>(current),
                static_cast<int>(target));

            *state = target;
            return true;
        }

    } // namespace AssetRuntime
} // namespace Dia
