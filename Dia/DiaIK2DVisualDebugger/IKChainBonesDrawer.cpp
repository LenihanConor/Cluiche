////////////////////////////////////////////////////////////////////////////////
// Filename: IKChainBonesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "IKChainBonesDrawer.h"

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
        IKChainBonesDrawer::IKChainBonesDrawer(
            const IKSolver&                      solver,
            const Dia::Rig2D::Skeleton&          skeleton,
            const Dia::Debug::DebugLayerManager& manager)
            : mSolver(solver)
            , mSkeleton(skeleton)
            , mManager(manager)
        {
        }

        Dia::Core::StringCRC IKChainBonesDrawer::GetLayerName() const
        {
            return Dia::Debug::LayerNames::kIKBones;
        }

        void IKChainBonesDrawer::Draw(Dia::Graphics::FrameData& frameData)
        {
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

                    frameData.RequestDraw(
                        worldTransforms[parentIdx].position,
                        worldTransforms[i].position,
                        Dia::Debug::DebugColourPalette::kGoal);
                }
            }
        }

    } // namespace IK2D
} // namespace Dia

#endif // DIA_DEBUG
