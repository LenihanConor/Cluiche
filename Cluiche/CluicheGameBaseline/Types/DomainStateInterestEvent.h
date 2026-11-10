#pragma once
#include <cstdint>
namespace Cluiche { namespace AppFlow {
    // Main (DebugPanelPageModule) -> Sim (VisualDebuggerModule) over the "DomainStateInterest" EventStream.
    // "The panel wants domain state for the next windowMs milliseconds"; re-send to keep the window open.
    struct DomainStateInterestEvent { uint32_t windowMs = 2000; };
} }
