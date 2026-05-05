#pragma once

namespace Dia
{
    namespace AssetRuntime
    {
        enum class AssetState
        {
            Registered,
            Staged,
            Loaded,
            Unloading
        };
    }
}
