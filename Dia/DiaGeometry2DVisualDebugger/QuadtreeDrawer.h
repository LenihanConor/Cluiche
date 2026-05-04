////////////////////////////////////////////////////////////////////////////////
// Filename: QuadtreeDrawer.h
// Description: IVisualDebugger that draws the nodes of a Quadtree.
//              Leaf nodes: kActive (white). Internal nodes: kInactive (grey).
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaGeometry2D/Spatial/Quadtree.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// QuadtreeDrawer<T>
//
// Holds a const reference to a Quadtree<T>. On Draw(), visits every node and
// draws its bounds: leaf=kActive, internal=kInactive.
//
// Layer: LayerNames::kGeoQuadtree   Priority: 5 (drawn under shapes)
////////////////////////////////////////////////////////////////////////////////
template<typename T, unsigned int MaxObjects = 2048>
class QuadtreeDrawer : public Dia::Debug::IVisualDebugger
{
public:
    QuadtreeDrawer(const Dia::Geometry2D::Quadtree<T, MaxObjects>& tree,
                   const Dia::Debug::DebugLayerManager&            manager)
        : mTree(tree)
        , mManager(manager)
    {}

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Geometry2D::Quadtree<T, MaxObjects>& mTree;
    const Dia::Debug::DebugLayerManager&            mManager;
};

} // namespace Dia::Geometry2DVisualDebugger

#include "DiaGeometry2DVisualDebugger/QuadtreeDrawer.inl"

#endif // DIA_DEBUG
