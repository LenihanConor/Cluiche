#pragma once
#include "DiaGeometry2D/Grid/CellCoord.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Pathfinding {
    struct PathResult {
        bool  success   = false;
        float totalCost = 0.0f;
        // FindPath reports failure (success == false) when the route is longer than this.
        static constexpr unsigned int kMaxCells = 256;
        Dia::Core::Containers::DynamicArrayC<Dia::Geometry2D::CellCoord, kMaxCells> cells;

        void ToWorldPositions(float cellSize,
                              Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 256>& outPositions) const;
    };
}
