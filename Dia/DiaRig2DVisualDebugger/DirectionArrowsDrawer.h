////////////////////////////////////////////////////////////////////////////////
// Filename: DirectionArrowsDrawer.h
// Description: Draws a Ray2D on each bone showing local +X axis in world space.
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
        // DirectionArrowsDrawer
        //
        // For each bone: computes direction = (cos(rotation), sin(rotation)) and draws
        // a Ray2D of length = bone.length * scale (or 4.0f * scale if length == 0).
        // Colour: kGoal (cyan).
        // Layer:   LayerNames::kRigArrows
        // Priority: 20
        ////////////////////////////////////////////////////////////////////////////////
        class DirectionArrowsDrawer : public Dia::Debug::IVisualDebugger
        {
        public:
            DirectionArrowsDrawer(
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
