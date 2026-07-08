////////////////////////////////////////////////////////////////////////////////
// Filename: BoneLinesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "BoneLinesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

namespace Dia::Rig2D
{

BoneLinesDrawer::BoneLinesDrawer(
    const Skeleton& skeleton,
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
    const Dia::Debug::DebugLayerManager& manager)
    : mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{
}

Dia::Core::StringCRC BoneLinesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kRigBones;
}

void BoneLinesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("rig.bones", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const int boneCount = mSkeleton.GetBoneCount();
    for (int i = 0; i < boneCount; ++i)
    {
        const Bone& bone = mSkeleton.GetBone(i);
        if (bone.parentIndex < 0)
            continue;

        const BoneTransform& boneWt   = mWorldTransforms[i];
        const BoneTransform& parentWt = mWorldTransforms[bone.parentIndex];

        draw.RequestDraw(
            parentWt.position,
            boneWt.position,
            Dia::Debug::DebugColourPalette::kActive);
    }
}

void BoneLinesDrawer::DrawImGui()
{
    ImGui::TextDisabled("Bones: %d", mSkeleton.GetBoneCount());
}

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
