#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

// Per-stage asset load status published by AssetServiceModule (MainPU)
// via ServiceStream so SimPU modules can poll completion without GetStatic().
struct AssetLoadStatus
{
    enum class State { kIdle, kLoading, kComplete, kFailed };

    Dia::Core::StringCRC stageId;
    State                state        = State::kIdle;
    unsigned int         loaded       = 0;
    unsigned int         total        = 0;
    unsigned int         failed       = 0;
};

} } // namespace Cluiche::AppFlow
