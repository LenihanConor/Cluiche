#pragma once

#include "DiaAssetRuntime/IAssetStateListener.h"
#include "DiaAssetRuntime/Messages/assetruntime_messages.h"
#include "DiaMessageBus/Bus.h"

namespace Dia
{
    namespace AssetRuntime
    {
        // Forwards IAssetStateListener callbacks onto the shared
        // Dia::MessageBus::Bus as AssetReadyEvent/AssetUnloadingEvent/
        // AssetLoadFailedEvent broadcasts.
        //
        // Registered via AssetRuntime::RegisterListener like any other
        // IAssetStateListener — it does not replace direct listener use.
        // AssetRuntime and IAssetStateListener stay free of DiaMessageBus
        // (SD-ARUN-001); only this adapter references it.
        class AssetRuntimeBusAdapter : public IAssetStateListener
        {
        public:
            explicit AssetRuntimeBusAdapter(Dia::MessageBus::Bus& bus);

            void OnAssetReady(const Dia::Core::StringCRC& assetId,
                               const Dia::Core::Containers::String512& resolvedPath) override;
            void OnAssetUnloading(const Dia::Core::StringCRC& assetId) override;
            void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) override;

        private:
            Dia::MessageBus::Bus& mBus;
        };
    }
}
