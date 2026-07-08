////////////////////////////////////////////////////////////////////////////////
// Filename: HexGridDrawer.h
// Description: IVisualDebugger that draws the hexagonal cell outlines of a
//              HexGrid (6 edge lines per cell, pointy-top orientation).
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaGeometry2D/Spatial/HexGrid.h>

namespace Dia::Core  { class IDebugContext; }

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
                  const Dia::Core::IDebugContext&           manager)
        : mGrid(grid)
        , mManager(manager)
    {}

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

    // Called by the owning stage module each frame with the externally-owned
    // selection. Pass nullptr to clear. The pointer is not stored across frames
    // — caller must pass it again each update.
    void SetSelection(const Dia::Geometry2D::HexCoord* selected)
    {
        mSelected = selected;
    }

private:
    const Dia::Geometry2D::HexGrid<T, MaxObjects>& mGrid;
    const Dia::Core::IDebugContext&           mManager;
    bool                                           mShowLabels{ false };
    const Dia::Geometry2D::HexCoord*               mSelected{ nullptr };
};

} // namespace Dia::Geometry2DVisualDebugger

#include "DiaGeometry2DVisualDebugger/HexGridDrawer.inl"

#endif // DIA_DEBUG
