#pragma once
#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace AssetRuntime {

// Synchronous JSON handler: opens the file to confirm it is reachable, then
// fires OnLoadComplete immediately. No content is decoded or stored — this
// handler exists solely to exercise AssetRuntime's type-dispatch and
// lifecycle paths for data assets.
class JsonPassthroughHandler : public Dia::AssetRuntime::IAssetTypeHandler
{
public:
    virtual void Load(const Dia::Core::StringCRC& assetId,
                      const Dia::Core::Containers::String512& resolvedPath,
                      IAssetLoadCallback* callback) override;

    virtual void Unload(const Dia::Core::StringCRC& assetId) override;
};

} } // namespace Dia::AssetRuntime
