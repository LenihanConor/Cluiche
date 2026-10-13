#pragma once
#include <cstdint>

namespace Cluiche { namespace AppFlow {

// One assembled console command line travelling MainPU -> SimPU over the
// "DebugConsoleLine" EventStream. The panel (Main PU) assembles the line and
// VisualDebuggerModule (Sim PU) executes it. At most 240 chars are used;
// the buffer is 256 so the payload stays trivially copyable.
struct DebugConsoleLineEvent
{
    static const int kMaxText = 256;

    uint32_t seq = 0;
    char     text[kMaxText] = {};
};

} } // namespace Cluiche::AppFlow
