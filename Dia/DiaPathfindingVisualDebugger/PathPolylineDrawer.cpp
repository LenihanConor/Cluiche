#include "PathPolylineDrawer.h"
#ifdef DIA_DEBUG

#include <DiaPathfinding/SquarePathGrid.h>
#include <DiaPathfinding/PathResult.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Pathfinding {

PathPolylineDrawer::PathPolylineDrawer(const SquarePathGrid& grid, const PathResult& result,
                                        const Dia::Core::IDebugContext& ctx, float cellSize)
    : mGrid(grid), mResult(result), mCtx(ctx), mCellSize(cellSize), mMarkerRadius(5.0f) {}

Dia::Core::StringCRC PathPolylineDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("Pathfinding.PathPolyline");
}

void PathPolylineDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!IsEnabled()) return;
    if (!mResult.success || mResult.cells.Size() < 2) return;

    Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 256> positions;
    mResult.ToWorldPositions(mCellSize, positions);

    const unsigned int n = positions.Size();

    for (unsigned int i = 0; i + 1 < n; ++i)
        draw.RequestDraw(positions[i], positions[i + 1], Dia::Debug::DebugColourPalette::kActive);

    const float radius = mMarkerRadius * mCtx.GetDebugScale();
    draw.RequestDraw(positions[0],      radius, Dia::Debug::DebugColourPalette::kGoal,   Dia::Debug::DebugColourPalette::kGoal);
    draw.RequestDraw(positions[n - 1],  radius, Dia::Debug::DebugColourPalette::kCapped, Dia::Debug::DebugColourPalette::kCapped);
}

} }
#endif // DIA_DEBUG
