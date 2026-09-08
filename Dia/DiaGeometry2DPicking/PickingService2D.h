////////////////////////////////////////////////////////////////////////////////
// Filename: PickingService2D.h
// Description: Stateless query service over registered IPickable2D objects.
//              Stateful registry (holds the pickable list), stateless queries.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGeometry2DPicking/IPickable2D.h>
#include <DiaGeometry2DPicking/PickHit2D.h>
#include <DiaPicking/PickResult.h>
#include <DiaPicking/PickLayer.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Geometry2D { class AARect; }

namespace Dia::Geometry2DPicking {

class PickingService2D
{
public:
    static constexpr unsigned int kMaxPickables = 64;

    void Register(IPickable2D* pickable);
    void Unregister(IPickable2D* pickable);

    Dia::Picking::PickResult<PickHit2D> Pick(
        const Dia::Maths::Vector2D& worldPos,
        Dia::Picking::PickLayerMask mask = static_cast<unsigned int>(Dia::Picking::PickLayer::kAll)) const;

    Dia::Picking::PickResult<PickHit2D> PickArea(
        const Dia::Geometry2D::AARect& worldRect,
        Dia::Picking::PickLayerMask mask = static_cast<unsigned int>(Dia::Picking::PickLayer::kAll)) const;

    void         Clear() { mPickables.RemoveAll(); }
    unsigned int GetPickableCount() const { return mPickables.Size(); }

private:
    Dia::Core::Containers::DynamicArrayC<IPickable2D*, kMaxPickables> mPickables;
};

} // namespace Dia::Geometry2DPicking
