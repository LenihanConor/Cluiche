#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Cluiche { namespace AppFlow {

// Discrete event payload: MainPU -> RenderPU.
// Carries stage lifecycle transitions that RenderPU needs to update its
// render state, replacing direct TestResultsRegistry cross-PU access.
struct MainToRenderEvent
{
    enum class Kind : uint8_t { kStageLifecycle };

    Kind                 kind      = Kind::kStageLifecycle;

    // Stage lifecycle data (replaces TestResultsRegistry cross-PU access)
    Dia::Core::StringCRC fromStage;
    Dia::Core::StringCRC toStage;
    unsigned int         frameCount = 0;
};

} } // namespace Cluiche::AppFlow
