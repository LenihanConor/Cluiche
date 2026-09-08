#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String512.h"

namespace Dia
{
    namespace AssetRuntime
    {
        // Passive observer interface for asset state transitions. Consuming
        // systems (DiaGraphics, DiaAudio, etc.) implement this to react to
        // asset lifecycle events without polling AssetRuntime.
        //
        // Listeners are raw interface pointers registered by game code —
        // AssetRuntime never owns or deletes them (SD-ARUN-001: no
        // ProcessingUnit/Module awareness). The caller must call
        // UnregisterListener before destroying the listener.
        //
        // Callbacks are invoked synchronously on the thread that calls
        // RequestStageLoad/RequestStageUnload.
        class IAssetStateListener
        {
        public:
            virtual ~IAssetStateListener() = default;

            // Called when an asset transitions to Staged (load requested,
            // resolved path available). The consumer should begin loading
            // content for assetId at resolvedPath.
            virtual void OnAssetReady(const Dia::Core::StringCRC& assetId,
                                       const Dia::Core::Containers::String512& resolvedPath) {}

            // Called when an asset transitions to Unloaded (ref count dropped
            // to zero). The consumer should release any content it holds for
            // assetId.
            virtual void OnAssetUnloading(const Dia::Core::StringCRC& assetId) {}

            // Called when an asset transitions to Failed (load attempt did
            // not succeed).
            virtual void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) {}
        };
    }
}
