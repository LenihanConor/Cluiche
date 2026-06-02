////////////////////////////////////////////////////////////////////////////////
// Filename: DirectionArrowsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DirectionArrowsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

#include <cmath>

namespace Dia::Rig2D
{

DirectionArrowsDrawer::DirectionArrowsDrawer(
    const Skeleton& skeleton,
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
    const Dia::Debug::DebugLayerManager& manager)
    : mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{
}

Dia::Core::StringCRC DirectionArrowsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kRigArrows;
}

void DirectionArrowsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    DIA_TRACE_ZONE("rig.arrows", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale     = mManager.GetDebugScale();
    const int   boneCount = mSkeleton.GetBoneCount();

    for (int i = 0; i < boneCount; ++i)
    {
        const Bone&          bone = mSkeleton.GetBone(i);
        const BoneTransform& wt   = mWorldTransforms[i];

        // Local +X in world space
        const Dia::Maths::Vector2D direction(
            std::cos(wt.rotation),
            std::sin(wt.rotation));

        const float length = (bone.length > 0.0f)
            ? bone.length * scale * mLengthMultiplier
            : 4.0f * scale * mLengthMultiplier;

        frameData.RequestDrawRay(
            wt.position,
            direction,
            length,
            Dia::Debug::DebugColourPalette::kGoal);
    }
}

void DirectionArrowsDrawer::DrawImGui()
{
    ImGui::SliderFloat("Length multiplier", &mLengthMultiplier, 0.1f, 5.0f);
}

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
