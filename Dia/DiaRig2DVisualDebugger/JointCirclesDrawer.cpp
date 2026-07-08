////////////////////////////////////////////////////////////////////////////////
// Filename: JointCirclesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JointCirclesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

namespace Dia::Rig2D
{

JointCirclesDrawer::JointCirclesDrawer(
    const Skeleton& skeleton,
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
    const Dia::Core::IDebugContext& manager)
    : mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{
}

Dia::Core::StringCRC JointCirclesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kRigJoints;
}

void JointCirclesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("rig.joints", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale      = mManager.GetDebugScale();
    const int   boneCount  = mSkeleton.GetBoneCount();

    // Build leaf lookup: isLeaf[i] == true if bone i has no children.
    bool isLeaf[kMaxBones] = {};
    for (int i = 0; i < boneCount; ++i)
        isLeaf[i] = true;

    for (int i = 0; i < boneCount; ++i)
    {
        const int parent = mSkeleton.GetBone(i).parentIndex;
        if (parent >= 0)
            isLeaf[parent] = false;
    }

    // Draw pass
    for (int i = 0; i < boneCount; ++i)
    {
        const Bone&          bone = mSkeleton.GetBone(i);
        const BoneTransform& wt   = mWorldTransforms[i];

        // Fixed pixel radii — positions are expected to be pre-scaled to screen space.
        if (bone.parentIndex < 0)
        {
            draw.RequestDraw(wt.position, 7.0f * mRadiusMultiplier,
                Dia::Debug::DebugColourPalette::kHealthy);
        }
        else if (isLeaf[i])
        {
            draw.RequestDraw(wt.position, 5.0f * mRadiusMultiplier,
                Dia::Debug::DebugColourPalette::kWarning);
        }
        else
        {
            draw.RequestDraw(wt.position, 5.0f * mRadiusMultiplier,
                Dia::Debug::DebugColourPalette::kActive);
        }
    }
}

void JointCirclesDrawer::DrawImGui()
{
    ImGui::SliderFloat("Radius multiplier", &mRadiusMultiplier, 0.1f, 5.0f);
}

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
