////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainBonesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "IKChainBonesDrawer.h"

#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaIK2D/IKSolver.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Dia::IK2D
{

IKChainBonesDrawer::IKChainBonesDrawer(
    const IKSolver&                      solver,
    const Dia::Rig2D::Skeleton&          skeleton,
    const Dia::Core::IDebugContext&      manager)
    : mSolver(solver)
    , mSkeleton(skeleton)
    , mManager(manager)
{
}

Dia::Core::StringCRC IKChainBonesDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kIKBones;
}

void IKChainBonesDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("ik.bones", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const int chainCount = mSolver.GetChainCount();
    const auto& worldTransforms = mSolver.GetWorldTransforms();

    for (int c = 0; c < chainCount; ++c)
    {
        const int startIdx = mSolver.GetChainStartBoneIndex(c);
        const int endIdx   = mSolver.GetChainEndBoneIndex(c);

        for (int i = startIdx; i <= endIdx; ++i)
        {
            const int parentIdx = mSkeleton.GetBone(i).parentIndex;
            if (parentIdx < 0)
                continue;

            draw.RequestDraw(
                worldTransforms[parentIdx].position,
                worldTransforms[i].position,
                Dia::Debug::DebugColourPalette::kGoal);
        }
    }
}

} // namespace Dia::IK2D

#endif // DIA_DEBUG
