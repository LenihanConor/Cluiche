#pragma once
#include "DiaPathfinding/CellCoord.h"

namespace Dia::Pathfinding {
    class IPathCostProvider {
    public:
        virtual ~IPathCostProvider() = default;
        virtual float GetCost(CellCoord from, CellCoord to) const = 0;
        static constexpr float kImpassableCost = -1.0f;
    };

    class FlatCostProvider : public IPathCostProvider {
    public:
        float GetCost(CellCoord /*from*/, CellCoord /*to*/) const override { return 1.0f; }
    };
}
