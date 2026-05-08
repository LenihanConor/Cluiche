////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainArrowsDrawer.h
// Description: Draws a Ray2D on each bone in each IK chain showing local +X
//              direction in world space. Pattern mirrors DirectionArrowsDrawer
//              but filtered to chain bones only.
// Feature spec: docs/specs/features/dia/diavisualdebugger/ik2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>

namespace Dia
{
    namespace Debug
    {
        class DebugLayerManager;
    }

    namespace IK2D
    {
        class IKSolver;
    }

    namespace Rig2D
    {
        class Skeleton;
    }
}

namespace Dia
{
    namespace IK2D
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
                const Dia::Debug::DebugLayerManager& manager);

            Dia::Core::StringCRC GetLayerName() const override;
            void Draw(Dia::Graphics::FrameData& frameData) override;

        private:
            const IKSolver&                      mSolver;
            const Dia::Rig2D::Skeleton&          mSkeleton;
            const Dia::Debug::DebugLayerManager& mManager;
        };

    } // namespace IK2D
} // namespace Dia

#endif // DIA_DEBUG
