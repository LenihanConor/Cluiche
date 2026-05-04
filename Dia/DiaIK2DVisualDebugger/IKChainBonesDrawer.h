////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainBonesDrawer.h
// Description: Draws a cyan line for each bone in each IK chain, distinguishing
//              IK-driven bones from FK bones.
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
        // IKChainBonesDrawer
        //
        // For each registered IK chain, draws a cyan line from parent-bone world
        // position to child-bone world position for every bone in the chain span.
        // Root bones (parentIndex < 0) are skipped.
        // Layer:    LayerNames::kIKBones
        // Priority: 10
        ////////////////////////////////////////////////////////////////////////////////
        class IKChainBonesDrawer : public Dia::Debug::IVisualDebugger
        {
        public:
            IKChainBonesDrawer(
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
