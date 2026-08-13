////////////////////////////////////////////////////////////////////////////////
// Filename: RestPoseDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "RestPoseDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaRig2D/Pose.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Dia::Rig2D
{

RestPoseDrawer::RestPoseDrawer(
    const Skeleton& skeleton,
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
    const Dia::Core::IDebugContext& manager)
    : mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{
}

Dia::Core::StringCRC RestPoseDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kRigRestPose;
}

void RestPoseDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("rig.rest_pose", ::Dia::Observation::Trace::Category::kDiaGraphics);
    // Build rest-pose world transforms (stack allocation — ~2.5 KB for 128 bones)
    Pose bindPose(mSkeleton);
    bindPose.SetToBindPose(mSkeleton);

    const BoneTransform identityRoot{
        Dia::Maths::Vector2D(0.0f, 0.0f),
        0.0f,
        Dia::Maths::Vector2D(1.0f, 1.0f)
    };

    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> restTransforms;
    bindPose.ComputeWorldTransforms(mSkeleton, identityRoot, restTransforms);

    const int boneCount = mSkeleton.GetBoneCount();
    for (int i = 0; i < boneCount; ++i)
    {
        const Bone& bone = mSkeleton.GetBone(i);
        if (bone.parentIndex < 0)
            continue;

        const BoneTransform& boneRest   = restTransforms[i];
        const BoneTransform& parentRest = restTransforms[bone.parentIndex];

        draw.RequestDraw(
            parentRest.position,
            boneRest.position,
            Dia::Debug::DebugColourPalette::kInactive);
    }
}

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
