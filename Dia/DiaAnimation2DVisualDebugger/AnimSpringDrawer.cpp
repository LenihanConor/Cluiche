////////////////////////////////////////////////////////////////////////////////
// Filename: AnimSpringDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaAnimation2DVisualDebugger/AnimSpringDrawer.h"

#ifdef DIA_DEBUG

#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaAnimation2D/SpringChain.h>
#include <DiaRig2D/Skeleton.h>
#include <DiaCore/DebugDraw/DebugColourPalette.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaCore/DebugDraw/IDebugContext.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>
#include <cmath>

namespace Dia::Animation2D {

AnimSpringDrawer::AnimSpringDrawer(
    const AnimationEvaluator&                                                    evaluator,
    const Dia::Rig2D::Skeleton&                                                  skeleton,
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms,
    const Dia::Core::IDebugContext&                                              manager)
    : mEvaluator(evaluator)
    , mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{}

Dia::Core::StringCRC AnimSpringDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kAnimSpring;
}

void AnimSpringDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("anim.spring", ::Dia::Observation::Trace::Category::kDiaGraphics);
    const float scale = mManager.GetDebugScale();
    const float circleRadius = 3.0f * scale;
    const float gravityRayLength = 3.0f * scale;

    const int sourceCount = mEvaluator.GetSourceCount();
    for (int i = 0; i < sourceCount; ++i)
    {
        const Dia::Core::StringCRC id    = mEvaluator.GetSourceId(i);
        const SpringChain*         chain = mEvaluator.GetSpringChain(id);
        if (chain == nullptr) continue;  // source is a clip player

        const int nodeCount = chain->GetNodeCount();
        for (int n = 0; n < nodeCount; ++n)
        {
            const Dia::Core::StringCRC boneId    = chain->GetNodeBoneId(n);
            const int                  boneIndex  = mSkeleton.FindBoneIndex(boneId);
            if (boneIndex < 0 || boneIndex >= (int)mWorldTransforms.Size()) continue;

            const Dia::Maths::Vector2D pos    = mWorldTransforms[boneIndex].position;
            const float                angVel = std::abs(chain->GetNodeAngularVelocity(n));

            Dia::Core::RGBA colour;
            if (angVel < mWarnThreshold)
                colour = Dia::Debug::DebugColourPalette::kHealthy;
            else if (angVel < mErrorThreshold)
                colour = Dia::Debug::DebugColourPalette::kWarning;
            else
                colour = Dia::Debug::DebugColourPalette::kError;

            draw.RequestDraw(pos, circleRadius, colour);
        }

        // Gravity indicator: draw ray from chain root bone in gravity direction
        if (chain->GetGravityStrength() > 0.0f && nodeCount > 0)
        {
            const Dia::Core::StringCRC rootBoneId    = chain->GetNodeBoneId(0);
            const int                  rootBoneIndex  = mSkeleton.FindBoneIndex(rootBoneId);
            if (rootBoneIndex >= 0 && rootBoneIndex < (int)mWorldTransforms.Size())
            {
                const Dia::Maths::Vector2D rootPos = mWorldTransforms[rootBoneIndex].position;
                const Dia::Maths::Vector2D gravDir = chain->GetGravityDirection();

                // gravDir is already normalised by SpringChain (enforced in constructor/SetGravity)
                draw.RequestDrawRay(
                    rootPos,
                    gravDir,
                    gravityRayLength,
                    Dia::Debug::DebugColourPalette::kInactive);
            }
        }
    }
}

void AnimSpringDrawer::DrawImGui()
{
    ImGui::SliderFloat("Warn threshold (rad/s)",  &mWarnThreshold,  0.0f, 10.0f);
    ImGui::SliderFloat("Error threshold (rad/s)", &mErrorThreshold, 0.0f, 20.0f);
}

} // namespace Dia::Animation2D

#endif // DIA_DEBUG
