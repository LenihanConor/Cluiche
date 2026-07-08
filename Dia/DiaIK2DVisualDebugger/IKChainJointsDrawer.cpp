////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainJointsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "IKChainJointsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaIK2D/IKSolver.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>

namespace Dia::IK2D
{

IKChainJointsDrawer::IKChainJointsDrawer(
    const IKSolver&                      solver,
    const Dia::Rig2D::Skeleton&          skeleton,
    const Dia::Core::IDebugContext&      manager)
    : mSolver(solver)
    , mSkeleton(skeleton)
    , mManager(manager)
{
}

Dia::Core::StringCRC IKChainJointsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kIKJoints;
}

void IKChainJointsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("ik.joints", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale      = mManager.GetDebugScale();
    const int   chainCount = mSolver.GetChainCount();
    const auto& worldTransforms = mSolver.GetWorldTransforms();

    for (int c = 0; c < chainCount; ++c)
    {
        const int startIdx = mSolver.GetChainStartBoneIndex(c);
        const int endIdx   = mSolver.GetChainEndBoneIndex(c);

        for (int i = startIdx; i <= endIdx; ++i)
        {
            const Dia::Maths::Vector2D& pos = worldTransforms[i].position;

            if (i == endIdx)
            {
                draw.RequestDraw(pos, 9.0f * mRadiusMultiplier,
                    Dia::Debug::DebugColourPalette::kHealthy);
            }
            else if (i == startIdx)
            {
                draw.RequestDraw(pos, 7.0f * mRadiusMultiplier,
                    Dia::Debug::DebugColourPalette::kGoal);
            }
            else
            {
                draw.RequestDraw(pos, 5.0f * mRadiusMultiplier,
                    Dia::Debug::DebugColourPalette::kGoal);
            }
        }
    }
}

void IKChainJointsDrawer::DrawImGui()
{
    ImGui::SliderFloat("Radius multiplier", &mRadiusMultiplier, 0.1f, 5.0f);
}

} // namespace Dia::IK2D

#endif // DIA_DEBUG
