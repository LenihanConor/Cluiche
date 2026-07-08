////////////////////////////////////////////////////////////////////////////////
// Filename: PickingService2D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGeometry2DPicking/PickingService2D.h"
#include <DiaGeometry2D/Shapes/AARect.h>
namespace Dia::Geometry2DPicking {

void PickingService2D::Register(IPickable2D* pickable)
{
    if (pickable == nullptr) return;
    if (mPickables.FindIndex(pickable) >= 0) return;
    if (!mPickables.IsFull())
        mPickables.Add(pickable);
}

void PickingService2D::Unregister(IPickable2D* pickable)
{
    if (pickable == nullptr) return;
    mPickables.RemoveFirst(pickable);
}

Dia::Picking::PickResult<PickHit2D> PickingService2D::Pick(
    const Dia::Maths::Vector2D& worldPos,
    Dia::Picking::PickLayerMask mask) const
{
    Dia::Picking::PickResult<PickHit2D> result;

    for (unsigned int i = 0; i < mPickables.Size(); ++i)
    {
        IPickable2D* pickable = mPickables[i];
        if (!Dia::Picking::LayerMatches(pickable->GetLayer(), mask)) continue;

        PickHit2D hit;
        if (pickable->TryPick(worldPos, hit))
            result.Add(hit);
    }

    return result;
}

Dia::Picking::PickResult<PickHit2D> PickingService2D::PickArea(
    const Dia::Geometry2D::AARect& worldRect,
    Dia::Picking::PickLayerMask mask) const
{
    Dia::Picking::PickResult<PickHit2D> result;
    Dia::Core::Containers::DynamicArrayC<PickHit2D, kMaxAreaHits> hitBuffer;

    for (unsigned int i = 0; i < mPickables.Size(); ++i)
    {
        IPickable2D* pickable = mPickables[i];
        if (!Dia::Picking::LayerMatches(pickable->GetLayer(), mask)) continue;

        hitBuffer.RemoveAll();
        pickable->PickArea(worldRect, hitBuffer);
        for (unsigned int j = 0; j < hitBuffer.Size(); ++j)
            result.Add(hitBuffer[j]);
    }

    return result;
}

} // namespace Dia::Geometry2DPicking
