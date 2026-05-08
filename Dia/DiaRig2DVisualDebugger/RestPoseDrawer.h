////////////////////////////////////////////////////////////////////////////////
// Filename: RestPoseDrawer.h
// Description: Draws the skeleton's bind/rest pose as a grey ghost overlay.
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
        // RestPoseDrawer
        //
        // Each Draw() call:
        //   1. Constructs a local Pose and calls SetToBindPose(skeleton).
        //   2. Calls ComputeWorldTransforms() with identity root to get rest-pose transforms.
        //   3. Draws grey lines for each non-root bone (rest-pose ghost).
        //
        // Stack-allocates DynamicArrayC<BoneTransform, kMaxBones> for rest transforms.
        // Layer:   LayerNames::kRigRestPose
        // Priority: 10
        ////////////////////////////////////////////////////////////////////////////////
        class RestPoseDrawer : public Dia::Debug::IVisualDebugger
        {
        public:
            RestPoseDrawer(
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
