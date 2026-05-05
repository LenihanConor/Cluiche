#pragma once

#include "DiaAssetRuntime/AssetScope.h"

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String512.h"

namespace Dia
{
    namespace AssetRuntime
    {
        // Absolute deploy path stored as a resolved string.
        // FilePath is alias-based with 32-char component limits and cannot
        // hold arbitrary absolute paths — String512 is the correct type here.
        struct RuntimeAssetEntry
        {
            Dia::Core::StringCRC                    mId;
            AssetScope                              mScope;
            Dia::Core::Containers::String512        mDeployPath; // absolute path, resolved from deploy root

            RuntimeAssetEntry()
                : mScope(AssetScope::kStage)
            {}
        };
    }
}
