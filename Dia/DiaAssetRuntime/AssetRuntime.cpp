#include "DiaAssetRuntime/AssetRuntime.h"

#include "DiaCore/FilePath/PathStore.h"
#include "DiaCore/FilePath/Path.h"

#include <DiaLogger/DiaLog.h>

namespace Dia
{
    namespace AssetRuntime
    {
        AssetRuntime::AssetRuntime()
        {}

        bool AssetRuntime::LoadManifest(const Dia::Core::FilePath& manifestPath)
        {
            RuntimeManifestLoader loader;
            if (!loader.Load(manifestPath, mAssetTable, mStageTable))
                return false;

            RegisterPathAliases();
            return true;
        }

        const Dia::Core::Containers::String512* AssetRuntime::ResolveAssetPath(const Dia::Core::StringCRC& assetId) const
        {
            const RuntimeAssetEntry* entry = mAssetTable.TryGetItemConst(assetId);
            if (!entry)
            {
                DIA_LOG_WARNING("AssetRuntime", "AssetRuntime::ResolveAssetPath: unknown asset ID '%s'", assetId.AsChar());
                return nullptr;
            }
            return &entry->mDeployPath;
        }

        void AssetRuntime::RegisterPathAliases()
        {
            // Register each asset ID as a PathStore alias pointing to its deploy directory,
            // so downstream code can resolve assets via the path alias system.
            // Asset manifest is authoritative — overwrites existing aliases with a warning.
            unsigned int count = mAssetTable.Size();
            for (unsigned int i = 0; i < count; ++i)
            {
                const RuntimeAssetEntry& entry = mAssetTable.GetItemByIndexConst(i);
                const Dia::Core::StringCRC& id = entry.mId;
                const char* pathStr = entry.mDeployPath.AsCStr();

                if (Dia::Core::PathStore::IsPathAliasRegistered(id))
                {
                    DIA_LOG_WARNING("AssetRuntime", "AssetRuntime: alias '%s' already registered — overwriting", id.AsChar());
                }

                Dia::Core::Path::String pathValue(pathStr);
                Dia::Core::PathStore::RegisterToStore(id, pathValue);
            }
        }

    } // namespace AssetRuntime
} // namespace Dia
