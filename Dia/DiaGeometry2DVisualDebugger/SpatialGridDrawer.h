////////////////////////////////////////////////////////////////////////////////
// Filename: SpatialGridDrawer.h
// Description: IVisualDebugger that draws the cell grid of a SpatialGrid.
//              Each cell is drawn as a grey rect (kInactive colour).
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// SpatialGridDrawer<T>
//
// Holds a const reference to a SpatialGrid<T>. On Draw(), iterates every cell
// and draws its AARect bounds in kInactive (grey).
//
// Layer: LayerNames::kGeoSpatialGrid   Priority: 5 (drawn under shapes)
////////////////////////////////////////////////////////////////////////////////
template<typename T, unsigned int MaxObjects = 2048>
class SpatialGridDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SpatialGridDrawer(const Dia::Geometry2D::SpatialGrid<T, MaxObjects>& grid,
                      const Dia::Debug::DebugLayerManager&               manager)
        : mGrid(grid)
        , mManager(manager)
    {}

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Geometry2D::SpatialGrid<T, MaxObjects>& mGrid;
    const Dia::Debug::DebugLayerManager&               mManager;
};

} // namespace Dia::Geometry2DVisualDebugger

#include "DiaGeometry2DVisualDebugger/SpatialGridDrawer.inl"

#endif // DIA_DEBUG
