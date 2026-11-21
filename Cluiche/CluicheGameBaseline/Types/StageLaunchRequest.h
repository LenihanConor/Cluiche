#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

// One-shot launch request sent by StageCatalogueModule (MainPU, stages.launch command) to SimPU.
// SimNavigationHandlerModule reads the stream and routes it like a menu navigation (stage + optional preset).
struct StageLaunchRequest
{
    Dia::Core::StringCRC stage;
    Dia::Core::StringCRC preset;   // empty = no preset
};

} } // namespace Cluiche::AppFlow
