////////////////////////////////////////////////////////////////////////////////
// Filename: BoneLabelsDrawer.h
// Description: Draws the bone name as a world-space text label at each joint.
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
        // BoneLabelsDrawer
        //
        // Calls RequestDrawText() for each bone with:
        //   position  = wt.position + (4, -4) * scale   (offset so text clears the circle)
        //   text      = bone.name.AsChar()
        //   fontSize  = min(12.0f * scale, kMaxFontSize) — clamped to prevent runaway
        //   colour    = kActive (white)
        //
        // Layer:   LayerNames::kRigLabels
        // Priority: 40
        ////////////////////////////////////////////////////////////////////////////////
        class BoneLabelsDrawer : public Dia::Debug::IVisualDebugger
        {
        public:
            static constexpr float kMaxFontSize = 24.0f;

            BoneLabelsDrawer(
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
