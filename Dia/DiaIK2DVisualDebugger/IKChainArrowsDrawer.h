////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainArrowsDrawer.h
// Description: Draws a Ray2D on each bone in each IK chain showing local +X
//              direction in world space. Pattern mirrors DirectionArrowsDrawer
//              but filtered to chain bones only.
// Feature spec: docs/specs/features/dia/diavisualdebugger/ik2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::Core  { class IDebugContext; }
namespace Dia::IK2D  { class IKSolver; }
namespace Dia::Rig2D { class Skeleton; }

namespace Dia::IK2D
{

////////////////////////////////////////////////////////////////////////////////
// IKChainArrowsDrawer
//
// For each bone in each IK chain:
//   direction = (cos(wt.rotation), sin(wt.rotation))
//   length    = bone.length * scale  (or 4.0f * scale if length == 0)
//   colour    = kGoal (cyan)
// Layer:    LayerNames::kIKArrows
// Priority: 20
////////////////////////////////////////////////////////////////////////////////
class IKChainArrowsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    IKChainArrowsDrawer(
        const IKSolver&                      solver,
        const Dia::Rig2D::Skeleton&          skeleton,
        const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    float mLengthMultiplier = 1.0f;
    const IKSolver&                      mSolver;
    const Dia::Rig2D::Skeleton&          mSkeleton;
    const Dia::Core::IDebugContext& mManager;
};

} // namespace Dia::IK2D

#endif // DIA_DEBUG
