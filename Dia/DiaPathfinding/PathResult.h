#pragma once
#include "DiaPathfinding/CellCoord.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Pathfinding {
    struct PathResult {
        bool  success   = false;
        float totalCost = 0.0f;
        Dia::Core::Containers::DynamicArrayC<CellCoord, 256> cells;

        void ToWorldPositions(float cellSize,
                              Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 256>& outPositions) const;
    };
}
