#pragma once
#include <cstdint>

namespace Dia::GridVisibility {

    enum class VisibilityState : uint8_t {
        Unexplored = 0,   // cell has never been seen by this group
        Revealed   = 1,   // cell was seen previously but is not currently visible
        Visible    = 2    // cell is within unblocked sight of at least one group observer
    };

} // namespace Dia::GridVisibility
