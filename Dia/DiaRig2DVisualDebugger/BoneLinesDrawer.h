////////////////////////////////////////////////////////////////////////////////
// Filename: BoneLinesDrawer.h
// Description: Draws a line from each bone's parent position to the bone
//              position (white lines, one per non-root bone).
// Feature spec: docs/specs/features/dia/diavisualdebugger/rig2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace Core
    {
        class IDebugContext;
    }
}

namespace Dia::Rig2D
{

////////////////////////////////////////////////////////////////////////////////
// BoneLinesDrawer
//
// Draws a white line from parent-bone world position to child-bone world
// position for every non-root bone in the skeleton.
// Layer:  LayerNames::kRigBones
// Priority: 10
////////////////////////////////////////////////////////////////////////////////
class BoneLinesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    BoneLinesDrawer(
        const Skeleton& skeleton,
        const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
        const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;

private:
    const Skeleton&                                                              mSkeleton;
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>&       mWorldTransforms;
    const Dia::Core::IDebugContext&                                              mManager;
};

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
