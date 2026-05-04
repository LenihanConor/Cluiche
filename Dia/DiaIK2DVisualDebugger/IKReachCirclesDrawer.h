////////////////////////////////////////////////////////////////////////////////
// Filename: IKReachCirclesDrawer.h
// Description: Draws a grey outline circle at each chain's start bone position
//              whose radius equals the total reach of the chain (sum of bone
//              lengths from startBone to endBone-1).
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
