////////////////////////////////////////////////////////////////////////////////
// Filename: BoneLabelsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "BoneLabelsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaObservation/Trace/DiaTrace.h>

#include <algorithm>

namespace Dia::Rig2D
{

BoneLabelsDrawer::BoneLabelsDrawer(
    const Skeleton& skeleton,
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
    const Dia::Core::IDebugContext& manager)
    : mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{
}

Dia::Core::StringCRC BoneLabelsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kRigLabels;
}

void BoneLabelsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("rig.labels", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale     = mManager.GetDebugScale();
    const float fontSize  = std::min(12.0f * scale * mFontSizeMultiplier, kMaxFontSize);
    const int   boneCount = mSkeleton.GetBoneCount();

    for (int i = 0; i < boneCount; ++i)
    {
        const Bone&          bone     = mSkeleton.GetBone(i);
        const BoneTransform& wt       = mWorldTransforms[i];
        const Dia::Maths::Vector2D labelPos(
            wt.position.x + 4.0f * scale,
            wt.position.y - 4.0f * scale);

        draw.RequestDrawText(
            labelPos,
            bone.name.AsChar(),
            fontSize,
            Dia::Debug::DebugColourPalette::kActive);
    }
}

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
