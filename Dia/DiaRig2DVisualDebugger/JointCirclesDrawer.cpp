////////////////////////////////////////////////////////////////////////////////
// Filename: JointCirclesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JointCirclesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

namespace Dia::Rig2D
{

JointCirclesDrawer::JointCirclesDrawer(
    const Skeleton& skeleton,
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
    const Dia::Debug::DebugLayerManager& manager)
    : mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{
}

Dia::Core::StringCRC JointCirclesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kRigJoints;
}

void JointCirclesDrawer::Draw(Dia::Graphics::FrameData& frameData)
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

        if (bone.parentIndex < 0)
        {
            // Root
            frameData.RequestDraw(
                wt.position,
                4.0f * scale * mRadiusMultiplier,
                Dia::Debug::DebugColourPalette::kHealthy);
        }
        else if (isLeaf[i])
        {
            // Leaf
            frameData.RequestDraw(
                wt.position,
                2.5f * scale * mRadiusMultiplier,
                Dia::Debug::DebugColourPalette::kWarning);
        }
        else
        {
            // Mid-chain
            frameData.RequestDraw(
                wt.position,
                2.5f * scale * mRadiusMultiplier,
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
