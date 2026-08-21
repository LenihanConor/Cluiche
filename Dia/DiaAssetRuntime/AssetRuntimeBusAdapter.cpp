#include "DiaAssetRuntime/AssetRuntimeBusAdapter.h"

namespace Dia
{
    namespace AssetRuntime
    {
        AssetRuntimeBusAdapter::AssetRuntimeBusAdapter(Dia::MessageBus::Bus& bus)
            : mBus(bus)
        {
        }

        void AssetRuntimeBusAdapter::OnAssetReady(const Dia::Core::StringCRC& assetId,
                                                    const Dia::Core::Containers::String512& resolvedPath)
        {
            mBus.Broadcast(AssetReadyEvent{ assetId, resolvedPath });
        }

        void AssetRuntimeBusAdapter::OnAssetUnloading(const Dia::Core::StringCRC& assetId)
        {
            mBus.Broadcast(AssetUnloadingEvent{ assetId });
        }

        void AssetRuntimeBusAdapter::OnAssetLoadFailed(const Dia::Core::StringCRC& assetId)
        {
            mBus.Broadcast(AssetLoadFailedEvent{ assetId });
        }
    }
}
