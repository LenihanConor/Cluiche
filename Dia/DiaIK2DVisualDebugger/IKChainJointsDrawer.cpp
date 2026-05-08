////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainJointsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "IKChainJointsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaIK2D/IKSolver.h>

namespace Dia
{
    namespace IK2D
    {
        IKChainJointsDrawer::IKChainJointsDrawer(
            const IKSolver&                      solver,
            const Dia::Rig2D::Skeleton&          skeleton,
            const Dia::Debug::DebugLayerManager& manager)
            : mSolver(solver)
            , mSkeleton(skeleton)
            , mManager(manager)
        {
        }

        Dia::Core::StringCRC IKChainJointsDrawer::GetLayerName() const
        {
            return Dia::Debug::LayerNames::kIKJoints;
        }

        void IKChainJointsDrawer::Draw(Dia::Graphics::FrameData& frameData)
        {
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
                        // End-effector: green, larger
                        frameData.RequestDraw(pos, 3.5f * scale,
                            Dia::Debug::DebugColourPalette::kHealthy);
                    }
                    else if (i == startIdx)
                    {
                        // Chain root: cyan
                        frameData.RequestDraw(pos, 3.0f * scale,
                            Dia::Debug::DebugColourPalette::kGoal);
                    }
                    else
                    {
                        // Mid-chain: cyan, smaller
                        frameData.RequestDraw(pos, 2.5f * scale,
                            Dia::Debug::DebugColourPalette::kGoal);
                    }
                }
            }
        }

    } // namespace IK2D
} // namespace Dia

#endif // DIA_DEBUG
