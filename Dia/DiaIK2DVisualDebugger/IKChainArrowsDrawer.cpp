////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainArrowsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "IKChainArrowsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaIK2D/IKSolver.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

#include <cmath>

namespace Dia::IK2D
{

IKChainArrowsDrawer::IKChainArrowsDrawer(
    const IKSolver&                      solver,
    const Dia::Rig2D::Skeleton&          skeleton,
    const Dia::Debug::DebugLayerManager& manager)
    : mSolver(solver)
    , mSkeleton(skeleton)
    , mManager(manager)
{
}

Dia::Core::StringCRC IKChainArrowsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kIKArrows;
}

void IKChainArrowsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("ik.arrows", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale      = mManager.GetDebugScale();
    const int   chainCount = mSolver.GetChainCount();
    const auto& worldTransforms = mSolver.GetWorldTransforms();

    for (int c = 0; c < chainCount; ++c)
    {
        const int startIdx = mSolver.GetChainStartBoneIndex(c);
        const int endIdx   = mSolver.GetChainEndBoneIndex(c);

        for (int i = startIdx; i <= endIdx; ++i)
        {
            const Dia::Rig2D::BoneTransform& wt   = worldTransforms[i];
            const Dia::Rig2D::Bone&          bone = mSkeleton.GetBone(i);

            const Dia::Maths::Vector2D direction(
                std::cos(wt.rotation),
                std::sin(wt.rotation));

            const float length = (bone.length > 0.0f)
                ? bone.length * scale * mLengthMultiplier
                : 4.0f * scale * mLengthMultiplier;

            draw.RequestDrawRay(
                wt.position,
                direction,
                length,
                Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

void IKChainArrowsDrawer::DrawImGui()
{
    ImGui::SliderFloat("Length multiplier", &mLengthMultiplier, 0.1f, 5.0f);
}

} // namespace Dia::IK2D

#endif // DIA_DEBUG
