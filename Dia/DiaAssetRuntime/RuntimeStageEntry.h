#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
    namespace AssetRuntime
    {
        struct RuntimeStageEntry
        {
            Dia::Core::StringCRC                                            mId;
            Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 64> mAssetIds;

            RuntimeStageEntry() {}
        };
    }
}
