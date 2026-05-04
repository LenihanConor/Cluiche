////////////////////////////////////////////////////////////////////////////////
// Filename: JointCirclesDrawer.h
// Description: Draws a colour-coded circle at each joint position:
//              root=green (larger), leaf=yellow, mid-chain=white.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rig2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/IVisualDebugger.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace Debug
    {
        class DebugLayerManager;
    }
}

namespace Dia
{
    namespace Rig2D
    {
        ////////////////////////////////////////////////////////////////////////////////
        // JointCirclesDrawer
        //
        // Draws circles at each joint with role-based colouring:
        //   Root bone:      radius 4.0f * scale, colour kHealthy (green)
        //   Leaf bones:     radius 2.5f * scale, colour kWarning (yellow)
        //   Mid-chain:      radius 2.5f * scale, colour kActive  (white)
        //
        // Leaf detection: build a bool[kMaxBones] lookup from parentIndex in one pass.
        // Layer:   LayerNames::kRigJoints
        // Priority: 10
        ////////////////////////////////////////////////////////////////////////////////
        class JointCirclesDrawer : public Dia::Debug::IVisualDebugger
        {
        public:
            JointCirclesDrawer(
                const Skeleton& skeleton,
                const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
                const Dia::Debug::DebugLayerManager& manager);

            Dia::Core::StringCRC GetLayerName() const override;
            void Draw(Dia::Graphics::FrameData& frameData) override;

        private:
            const Skeleton&                                                              mSkeleton;
            const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>&       mWorldTransforms;
            const Dia::Debug::DebugLayerManager&                                         mManager;
        };

    } // namespace Rig2D
} // namespace Dia

#endif // DIA_DEBUG
