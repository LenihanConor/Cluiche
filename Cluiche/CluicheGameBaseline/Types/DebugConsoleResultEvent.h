#pragma once
#include <cstdint>

namespace Cluiche { namespace AppFlow {

// Result of one console command travelling SimPU -> MainPU over the
// "DebugConsoleResult" EventStream. seq matches the originating
// DebugConsoleLineEvent; text echoes the command line.
struct DebugConsoleResultEvent
{
    static const int kMaxText = 256;

    uint32_t seq        = 0;
    int32_t  exitCode   = 0;
    uint8_t  parseError = 0;
    char     text[kMaxText] = {};
};

} } // namespace Cluiche::AppFlow
