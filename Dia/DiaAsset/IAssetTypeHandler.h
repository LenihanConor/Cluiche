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

            // Releases ownership of the asset. May be called from any thread.
            // Reclamation of underlying resources may be deferred — handlers
            // owning thread-affined resources (e.g. GPU textures bound to a
            // render context, audio buffers tied to a mixer thread) are
            // responsible for ensuring the actual destruction happens on the
            // correct thread. The runtime guarantees only that Unload is called
            // exactly once per outstanding Load.
            virtual void Unload(const Dia::Core::StringCRC& assetId) = 0;
        };
    }
}
