////////////////////////////////////////////////////////////////////////////////
// Filename: RestPoseDrawer.h
// Description: Draws the skeleton's bind/rest pose as a grey ghost overlay.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rig2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Core { class IDebugContext; }

namespace Dia::Rig2D
{

////////////////////////////////////////////////////////////////////////////////
// RestPoseDrawer
//
// Each Draw() call:
//   1. Constructs a local Pose and calls SetToBindPose(skeleton).
//   2. Calls ComputeWorldTransforms() with identity root to get rest-pose transforms.
//   3. Draws grey lines for each non-root bone (rest-pose ghost).
//
// Stack-allocates DynamicArrayC<BoneTransform, kMaxBones> for rest transforms.
// Layer:   LayerNames::kRigRestPose
// Priority: 10
////////////////////////////////////////////////////////////////////////////////
class RestPoseDrawer : public Dia::Debug::IVisualDebugger
{
public:
    RestPoseDrawer(
        const Skeleton& skeleton,
        const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
        const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const Skeleton&                                                              mSkeleton;
    [[maybe_unused]] const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>&       mWorldTransforms;
    [[maybe_unused]] const Dia::Core::IDebugContext&                                              mManager;
};

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
