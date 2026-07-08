////////////////////////////////////////////////////////////////////////////////
// Filename: IKReachCirclesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "IKReachCirclesDrawer.h"

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

IKReachCirclesDrawer::IKReachCirclesDrawer(
    const IKSolver&                      solver,
    const Dia::Rig2D::Skeleton&          skeleton,
    const Dia::Core::IDebugContext&      manager)
    : mSolver(solver)
    , mSkeleton(skeleton)
    , mManager(manager)
{
}

Dia::Core::StringCRC IKReachCirclesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kIKReach;
}

void IKReachCirclesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("ik.reach", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale      = mManager.GetDebugScale();
    const int   chainCount = mSolver.GetChainCount();
    const auto& worldTransforms = mSolver.GetWorldTransforms();

    for (int c = 0; c < chainCount; ++c)
    {
        const int startIdx = mSolver.GetChainStartBoneIndex(c);
        const int endIdx   = mSolver.GetChainEndBoneIndex(c);

        // Sum bone lengths from startIdx to endIdx-1 (exclusive of the end bone)
        float reachRadius = 0.0f;
        for (int i = startIdx; i < endIdx; ++i)
        {
            reachRadius += mSkeleton.GetBone(i).length;
        }

        if (reachRadius > 0.0f)
        {
            const Dia::Maths::Vector2D& screenPos = worldTransforms[startIdx].position;
            draw.RequestDraw(
                screenPos,
                reachRadius * scale,
                Dia::Debug::DebugColourPalette::kInactive);
        }
    }
}

void IKReachCirclesDrawer::DrawImGui()
{
    ImGui::TextDisabled("Reach circles — chain count: %d", mSolver.GetChainCount());
}

} // namespace Dia::IK2D

#endif // DIA_DEBUG
