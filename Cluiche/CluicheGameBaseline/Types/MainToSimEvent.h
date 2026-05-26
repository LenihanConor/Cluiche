#pragma once
#include <cstdint>
#include "Types/InputEvent.h"

namespace Cluiche { namespace AppFlow {

// Composite event payload: MainPU -> SimPU (sent as discrete batched events).
// Consolidates the former per-concern InputToSim stream into a single PU-pair
// event channel. Future kinds (kStageLoad) extend this enum without adding streams.
struct MainToSimEvent
{
    enum class Kind : uint8_t
    {
        kInput,      // OS input event (keyboard / mouse / gamepad)
        // kStageLoad  — future: stage lifecycle hint from MainPU to SimPU
    };

    Kind       kind  = Kind::kInput;
    InputEvent input;  // valid when kind == kInput
};

} } // namespace Cluiche::AppFlow
