#pragma once
#include <cstdint>
#include "Types/UICommand.h"

namespace Cluiche { namespace AppFlow {

// Composite event payload: SimPU -> MainPU (sent as discrete batched events).
// Consolidates the former per-concern SimToUI stream into a single PU-pair
// event channel. Future kinds extend this enum without adding streams.
struct SimToMainEvent
{
    enum class Kind : uint8_t
    {
        kUICommand,   // HUD / page update command (FPS, Score, etc.)
    };

    Kind      kind = Kind::kUICommand;
    UICommand uiCommand;  // valid when kind == kUICommand
};

} } // namespace Cluiche::AppFlow
