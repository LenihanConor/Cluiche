////////////////////////////////////////////////////////////////////////////////
// Filename: AnimBlendWeightsDrawer.h
// Description: IVisualDebugger that draws a text label near the skeleton root
//              for each layer in the PoseBlendStack, showing layer ID, weight,
//              and priority.
// Feature spec: docs/specs/features/dia/diavisualdebugger/animation2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Animation2D { class AnimationEvaluator; } }
namespace Dia { namespace Rig2D      { class Skeleton; } }
namespace Dia { namespace Debug      { class DebugLayerManager; } }

namespace Dia { namespace Animation2D {

////////////////////////////////////////////////////////////////////////////////
// AnimBlendWeightsDrawer
//
// Stacks text labels to the RIGHT of the skeleton root bone (bone index 0).
// One label per blend layer.
//   Text format: "layerId w=0.75 p=1"
//   weight > 0.05 → kActive (white); else kInactive (grey)
//
// Layer: LayerNames::kAnimBlendWeights   Priority: 40
////////////////////////////////////////////////////////////////////////////////
class AnimBlendWeightsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    static constexpr float kMaxFontSize = 20.0f;

    AnimBlendWeightsDrawer(
        const AnimationEvaluator&                                                    evaluator,
        const Dia::Rig2D::Skeleton&                                                  skeleton,
        const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms,
        const Dia::Debug::DebugLayerManager&                                         manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const AnimationEvaluator&                                                    mEvaluator;
    const Dia::Rig2D::Skeleton&                                                  mSkeleton;
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& mWorldTransforms;
    const Dia::Debug::DebugLayerManager&                                         mManager;
};

} } // namespace Dia::Animation2D

#endif // DIA_DEBUG
