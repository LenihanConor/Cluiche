////////////////////////////////////////////////////////////////////////////////
// Filename: BVHDrawer.h
// Description: IVisualDebugger that draws BVH nodes coloured by depth.
//              Only draws if BVH::IsBuilt() returns true.
//              Depth colours cycle through: kActive -> kGoal -> kHealthy -> kWarning
// Feature spec: docs/specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaGeometry2D/Spatial/BVH.h>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// BVHDrawer<T>
//
// Holds a const reference to a BVH<T>. On Draw(), visits every node if IsBuilt()
// and colours each rect by depth % 4:
//   0 = kActive, 1 = kGoal, 2 = kHealthy, 3 = kWarning
//
// Layer: LayerNames::kGeoBVH   Priority: 5 (drawn under shapes)
////////////////////////////////////////////////////////////////////////////////
template<typename T, unsigned int MaxObjects = 2048>
class BVHDrawer : public Dia::Debug::IVisualDebugger
{
public:
    BVHDrawer(const Dia::Geometry2D::BVH<T, MaxObjects>& bvh,
              const Dia::Debug::DebugLayerManager&        manager)
        : mBVH(bvh)
        , mManager(manager)
    {}

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Dia::Geometry2D::BVH<T, MaxObjects>& mBVH;
    const Dia::Debug::DebugLayerManager&       mManager;
};

} // namespace Dia::Geometry2DVisualDebugger

#include "DiaGeometry2DVisualDebugger/BVHDrawer.inl"

#endif // DIA_DEBUG
