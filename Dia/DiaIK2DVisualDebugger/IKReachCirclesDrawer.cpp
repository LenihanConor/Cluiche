////////////////////////////////////////////////////////////////////////////////
// Filename: IKReachCirclesDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "IKReachCirclesDrawer.h"

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
        IKReachCirclesDrawer::IKReachCirclesDrawer(
            const IKSolver&                      solver,
            const Dia::Rig2D::Skeleton&          skeleton,
            const Dia::Debug::DebugLayerManager& manager)
            : mSolver(solver)
            , mSkeleton(skeleton)
            , mManager(manager)
        {
        }

        Dia::Core::StringCRC IKReachCirclesDrawer::GetLayerName() const
        {
            return Dia::Debug::LayerNames::kIKReach;
        }

        void IKReachCirclesDrawer::Draw(Dia::Graphics::FrameData& frameData)
        {
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
                    const Dia::Maths::Vector2D& startPos = worldTransforms[startIdx].position;
                    frameData.RequestDraw(
                        startPos,
                        reachRadius * scale,
                        Dia::Debug::DebugColourPalette::kInactive);
                }
            }
        }

    } // namespace IK2D
} // namespace Dia

#endif // DIA_DEBUG
