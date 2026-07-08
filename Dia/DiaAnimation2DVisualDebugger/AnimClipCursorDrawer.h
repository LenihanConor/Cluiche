////////////////////////////////////////////////////////////////////////////////
// Filename: AnimClipCursorDrawer.h
// Description: IVisualDebugger that draws a text label near the skeleton root
//              for each registered AnimClipPlayer, showing clip ID and normalised
//              playback time (or "stopped").
// Feature spec: docs/specs/features/dia/diavisualdebugger/animation2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Animation2D { class AnimationEvaluator; }
namespace Dia::Rig2D      { class Skeleton; }
namespace Dia::Debug      { class DebugLayerManager; }

namespace Dia::Animation2D {

////////////////////////////////////////////////////////////////////////////////
// AnimClipCursorDrawer
//
// Stacks text labels to the LEFT of the skeleton root bone (bone index 0).
// One label per registered AnimClipPlayer source.
//   Playing:  "[id] 0.45"   — white  (kActive)
//   Stopped:  "[id] stopped" — grey   (kInactive)
//
// Layer: LayerNames::kAnimClipCursor   Priority: 40
////////////////////////////////////////////////////////////////////////////////
class AnimClipCursorDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr float kMaxFontSize = 20.0f;

    AnimClipCursorDrawer(
        const AnimationEvaluator&                                                    evaluator,
        const Dia::Rig2D::Skeleton&                                                  skeleton,
        const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms,
        const Dia::Debug::DebugLayerManager&                                         manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    bool mShowStopped = true;
    const AnimationEvaluator&                                                    mEvaluator;
    const Dia::Rig2D::Skeleton&                                                  mSkeleton;
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& mWorldTransforms;
    const Dia::Debug::DebugLayerManager&                                         mManager;
};

} // namespace Dia::Animation2D

#endif // DIA_DEBUG
