#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
    namespace FlowField
    {
        // A single cell entry in a FlowField.
        // direction is normalised; {0,0} if the cell cannot reach the goal.
        struct FlowCell
        {
            Dia::Maths::Vector2D direction;  // normalised flow vector; zero if unreachable
            bool reachable = false;
        };
    }
}
