#pragma once
namespace Cluiche { namespace AppFlow {
    // Signal from VisualDebuggerModule (SimPU) -> DebugPanelPageModule (MainPU).
    // Each event means "flip panel visibility".
    struct DebugPanelToggleEvent {};
} }
