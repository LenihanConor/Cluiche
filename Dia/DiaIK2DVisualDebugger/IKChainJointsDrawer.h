////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainJointsDrawer.h
// Description: Draws a colour-coded circle at each joint position inside IK chains.
//              End bone: green (kHealthy). Start bone and mid-chain: cyan (kGoal).
// Feature spec: docs/specs/features/dia/diavisualdebugger/ik2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia::Debug { class DebugLayerManager; }
namespace Dia::IK2D  { class IKSolver; }
namespace Dia::Rig2D { class Skeleton; }

namespace Dia::IK2D
{

////////////////////////////////////////////////////////////////////////////////
// IKChainJointsDrawer
//
// For each bone in each IK chain, draws a circle sized by visual role:
//   End bone   (i == endIdx):   radius 3.5f * scale, colour kHealthy (green)
//   Start bone (i == startIdx): radius 3.0f * scale, colour kGoal    (cyan)
//   Mid-chain:                  radius 2.5f * scale, colour kGoal    (cyan)
// Layer:    LayerNames::kIKJoints
// Priority: 10
////////////////////////////////////////////////////////////////////////////////
class IKChainJointsDrawer : public Dia::Debug::IVisualDebugger
{
public:
    IKChainJointsDrawer(
        const IKSolver&                      solver,
        const Dia::Rig2D::Skeleton&          skeleton,
        const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    float mRadiusMultiplier = 1.0f;
    const IKSolver&                      mSolver;
    [[maybe_unused]] const Dia::Rig2D::Skeleton&          mSkeleton;
    const Dia::Debug::DebugLayerManager& mManager;
};

} // namespace Dia::IK2D

#endif // DIA_DEBUG
