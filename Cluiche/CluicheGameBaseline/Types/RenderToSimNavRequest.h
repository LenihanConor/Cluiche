#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

// One-shot navigation intent sent from a RenderPU module to SimPU.
// SimNavigationHandlerModule reads this stream and calls TransitionTo().
struct RenderToSimNavRequest
{
    Dia::Core::StringCRC target;
};

} } // namespace Cluiche::AppFlow
