#pragma once

#include "DiaAssetRuntime/RuntimeManifestLoader.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String512.h"

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

        private:
            void RegisterPathAliases();

            RuntimeManifestLoader::AssetTable mAssetTable;
            RuntimeManifestLoader::StageTable mStageTable;
        };
    }
}
