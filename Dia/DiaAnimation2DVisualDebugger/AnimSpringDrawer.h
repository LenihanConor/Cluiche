////////////////////////////////////////////////////////////////////////////////
// Filename: AnimSpringDrawer.h
// Description: IVisualDebugger that draws per-node spring state circles coloured
//              by angular velocity magnitude, and a gravity direction indicator
//              from each spring chain root.
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
// AnimSpringDrawer
//
// For each SpringChain source registered with the AnimationEvaluator:
//   - Draws a circle at each node's bone world position, coloured by |angVel|:
//       < 0.5 rad/s  → kHealthy (green)   at rest / converged
//       0.5–5.0 rad/s → kWarning (yellow)  active oscillation
//       >= 5.0 rad/s  → kError   (red)     fast / potentially unstable
//   - Draws a ray from the chain root bone in the gravity direction (if strength > 0).
//
// Layer: LayerNames::kAnimSpring   Priority: 20
////////////////////////////////////////////////////////////////////////////////
class AnimSpringDrawer : public Dia::Debug::IVisualDebugger
{
public:
    AnimSpringDrawer(
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
