#pragma once
#include "DiaGeometry2D/Grid/CellCoord.h"

namespace Dia::Pathfinding {
    class IPathCostProvider {
    public:
        virtual ~IPathCostProvider() = default;
        virtual float GetCost(Dia::Geometry2D::CellCoord from, Dia::Geometry2D::CellCoord to) const = 0;
        static constexpr float kImpassableCost = -1.0f;
    };

    class FlatCostProvider : public IPathCostProvider {
    public:
        float GetCost(Dia::Geometry2D::CellCoord /*from*/, Dia::Geometry2D::CellCoord /*to*/) const override { return 1.0f; }
    };
}
