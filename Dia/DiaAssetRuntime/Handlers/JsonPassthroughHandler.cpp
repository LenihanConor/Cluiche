#include "DiaAssetRuntime/Handlers/JsonPassthroughHandler.h"
#include <DiaObservation/Log/DiaLog.h>
#include <cstdio>

namespace Dia { namespace AssetRuntime {

void JsonPassthroughHandler::Load(const Dia::Core::StringCRC& assetId,
                                   const Dia::Core::Containers::String512& resolvedPath,
                                   IAssetLoadCallback* callback)
{
    FILE* f = nullptr;
    fopen_s(&f, resolvedPath.AsCStr(), "rb");
    if (!f)
    {
        DIA_LOG_ERROR("AssetRuntime", "JsonPassthroughHandler: file not found: %s",
            resolvedPath.AsCStr());
        if (callback)
            callback->OnLoadFailed(assetId, "file not found");
        return;
    }
    fclose(f);
    DIA_LOG_INFO("AssetRuntime", "JsonPassthroughHandler: loaded '%s'", resolvedPath.AsCStr());
    if (callback)
        callback->OnLoadComplete(assetId);
}

void JsonPassthroughHandler::Unload(const Dia::Core::StringCRC& /*assetId*/)
{
    // No content stored — nothing to release.
}

} } // namespace Dia::AssetRuntime
