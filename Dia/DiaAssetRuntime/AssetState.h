#pragma once

namespace Dia
{
    namespace AssetRuntime
    {
        enum class AssetState
        {
            Null,
            Staged,
            Loading,
            Loaded,
            Failed,
            Unloaded
        };
    }
}
