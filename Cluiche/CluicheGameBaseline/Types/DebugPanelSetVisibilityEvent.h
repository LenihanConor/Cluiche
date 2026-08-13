#pragma once
namespace Cluiche { namespace AppFlow {
    // Signal from VisualDebuggerModule (SimPU) -> DebugPanelPageModule (MainPU).
    // Explicitly sets panel visibility (true = show, false = hide).
    // Used for automatic stage-transition behavior.
    struct DebugPanelSetVisibilityEvent
    {
        bool visible;
    };
} }
