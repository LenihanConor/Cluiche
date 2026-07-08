////////////////////////////////////////////////////////////////////////////////
// Filename: BoneLabelsDrawer.h
// Description: Draws the bone name as a world-space text label at each joint.
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
// BoneLabelsDrawer
//
// Calls RequestDrawText() for each bone with:
//   position  = wt.position + (4, -4) * scale   (offset so text clears the circle)
//   text      = bone.name.AsChar()
//   fontSize  = min(12.0f * scale, kMaxFontSize) — clamped to prevent runaway
//   colour    = kActive (white)
//
// Layer:   LayerNames::kRigLabels
// Priority: 40
////////////////////////////////////////////////////////////////////////////////
class BoneLabelsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr float kMaxFontSize = 24.0f;

    BoneLabelsDrawer(
        const Skeleton& skeleton,
        const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
        const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    float mFontSizeMultiplier = 1.0f;
    const Skeleton&                                                              mSkeleton;
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>&       mWorldTransforms;
    const Dia::Core::IDebugContext&                                              mManager;
};

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
