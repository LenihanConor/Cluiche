////////////////////////////////////////////////////////////////////////////////
// Filename: IKReachCirclesDrawer.h
// Description: Draws a grey outline circle at each chain's start bone position
//              whose radius equals the total reach of the chain (sum of bone
//              lengths from startBone to endBone-1).
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
// IKReachCirclesDrawer
//
// For each chain:
//   reachRadius = sum of bone.length for i in [startIdx, endIdx-1]
//   If reachRadius > 0: draw outline circle at startBone world position
//   Colour: kInactive (grey)
// Layer:    LayerNames::kIKReach
// Priority: 30
////////////////////////////////////////////////////////////////////////////////
class IKReachCirclesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    IKReachCirclesDrawer(
        const IKSolver&                      solver,
        const Dia::Rig2D::Skeleton&          skeleton,
        const Dia::Core::IDebugContext& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Core::IDebugDraw& draw) override;
    void DrawImGui() override;

private:
    const IKSolver&                      mSolver;
    const Dia::Rig2D::Skeleton&          mSkeleton;
    const Dia::Core::IDebugContext& mManager;
};

} // namespace Dia::IK2D

#endif // DIA_DEBUG
