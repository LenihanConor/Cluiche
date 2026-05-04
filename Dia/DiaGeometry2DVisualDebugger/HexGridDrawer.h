////////////////////////////////////////////////////////////////////////////////
// Filename: HexGridDrawer.h
// Description: IVisualDebugger that draws the hexagonal cell outlines of a
//              HexGrid (6 edge lines per cell, pointy-top orientation).
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// HexGridDrawer<T>
//
// Holds a const reference to a HexGrid<T>. On Draw(), iterates all valid
// HexCoord cells and draws 6 edge lines per hexagon using kInactive (grey).
//
// Pointy-top hexagon corner angles: 30, 90, 150, 210, 270, 330 degrees.
//
// Layer: LayerNames::kGeoSpatialGrid   Priority: 5 (drawn under shapes)
// (Reuses the kGeoSpatialGrid layer name — HexGrid is a spatial structure)
////////////////////////////////////////////////////////////////////////////////
template<typename T, unsigned int MaxObjects = 2048>
class HexGridDrawer : public Dia::Debug::IVisualDebugger
{
public:
    HexGridDrawer(const Dia::Geometry2D::HexGrid<T, MaxObjects>& grid,
                  const Dia::Debug::DebugLayerManager&           manager)
        : mGrid(grid)
        , mManager(manager)
    {}

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Geometry2D::HexGrid<T, MaxObjects>& mGrid;
    const Dia::Debug::DebugLayerManager&           mManager;
};

} // namespace Dia::Geometry2DVisualDebugger

#include "DiaGeometry2DVisualDebugger/HexGridDrawer.inl"

#endif // DIA_DEBUG
