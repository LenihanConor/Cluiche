#include "DiaPathfinding/PathResult.h"

namespace Dia::Pathfinding {

void PathResult::ToWorldPositions(float cellSize,
    Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 256>& outPositions) const
{
    outPositions.RemoveAll();
    for (unsigned int i = 0; i < cells.Size(); ++i)
    {
        const CellCoord& c = cells[i];
        outPositions.Add(Dia::Maths::Vector2D(
            static_cast<float>(c.x) * cellSize,
            static_cast<float>(c.y) * cellSize));
    }
}

} // namespace Dia::Pathfinding
