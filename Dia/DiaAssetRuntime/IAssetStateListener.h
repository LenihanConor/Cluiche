#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String512.h"

namespace Dia
{
    namespace AssetRuntime
    {
        class IAssetStateListener
        {
        public:
            virtual ~IAssetStateListener() = default;

            // Called when an asset transitions Registered->Staged (load can begin).
            // resolvedPath is the absolute deploy path for the asset.
            virtual void OnAssetReady(const Dia::Core::StringCRC& assetId,
                                      const Dia::Core::Containers::String512& resolvedPath) = 0;

            // Called when an asset transitions to Unloading (content must be released).
            virtual void OnAssetUnloading(const Dia::Core::StringCRC& assetId) = 0;
        };
    }
}
