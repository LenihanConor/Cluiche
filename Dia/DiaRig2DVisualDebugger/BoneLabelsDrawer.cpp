////////////////////////////////////////////////////////////////////////////////
// Filename: BoneLabelsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "BoneLabelsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>

#include <algorithm>

namespace Dia
{
    namespace Rig2D
    {
        BoneLabelsDrawer::BoneLabelsDrawer(
            const Skeleton& skeleton,
            const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
            const Dia::Debug::DebugLayerManager& manager)
            : mSkeleton(skeleton)
            , mWorldTransforms(worldTransforms)
            , mManager(manager)
        {
        }

        Dia::Core::StringCRC BoneLabelsDrawer::GetLayerName() const
        {
            return Dia::Debug::LayerNames::kRigLabels;
        }

        void BoneLabelsDrawer::Draw(Dia::Graphics::FrameData& frameData)
        {
            const float scale     = mManager.GetDebugScale();
            const float fontSize  = std::min(12.0f * scale, kMaxFontSize);
            const int   boneCount = mSkeleton.GetBoneCount();

            for (int i = 0; i < boneCount; ++i)
            {
                const Bone&          bone     = mSkeleton.GetBone(i);
                const BoneTransform& wt       = mWorldTransforms[i];
                const Dia::Maths::Vector2D labelPos(
                    wt.position.x + 4.0f * scale,
                    wt.position.y - 4.0f * scale);

                frameData.RequestDrawText(
                    labelPos,
                    bone.name.AsChar(),
                    fontSize,
                    Dia::Debug::DebugColourPalette::kActive);
            }
        }

    } // namespace Rig2D
} // namespace Dia

#endif // DIA_DEBUG
