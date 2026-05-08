#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String512.h"

namespace Dia
{
    namespace AssetRuntime
    {
        class IAssetLoadCallback
        {
        public:
            virtual ~IAssetLoadCallback() = default;

            virtual void OnLoadComplete(const Dia::Core::StringCRC& assetId) = 0;
            virtual void OnLoadFailed(const Dia::Core::StringCRC& assetId, const char* reason) = 0;
        };

        class IAssetTypeHandler
        {
        public:
            virtual ~IAssetTypeHandler() = default;

            virtual void Load(const Dia::Core::StringCRC& assetId,
                              const Dia::Core::Containers::String512& resolvedPath,
                              IAssetLoadCallback* callback) = 0;

            virtual void Unload(const Dia::Core::StringCRC& assetId) = 0;
        };
    }
}
