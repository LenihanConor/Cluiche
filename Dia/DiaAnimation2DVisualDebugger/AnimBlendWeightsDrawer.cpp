////////////////////////////////////////////////////////////////////////////////
// Filename: AnimBlendWeightsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaAnimation2DVisualDebugger/AnimBlendWeightsDrawer.h"

#ifdef DIA_DEBUG

#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaAnimation2D/PoseBlendStack.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>

#include <DiaObservation/Trace/DiaTrace.h>
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace Dia::Animation2D {

AnimBlendWeightsDrawer::AnimBlendWeightsDrawer(
    const AnimationEvaluator&                                                    evaluator,
    const Dia::Rig2D::Skeleton&                                                  skeleton,
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms,
    const Dia::Debug::DebugLayerManager&                                         manager)
    : mEvaluator(evaluator)
    , mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{}

Dia::Core::StringCRC AnimBlendWeightsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kAnimBlendWeights;
}

void AnimBlendWeightsDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    DIA_TRACE_ZONE("anim.blend_weights", ::Dia::Observation::Trace::Category::kDiaGraphics);
    if (mWorldTransforms.Size() == 0) return;

    const float scale    = mManager.GetDebugScale();
    const float fontSize = std::min(10.0f * scale, kMaxFontSize);

    const Dia::Maths::Vector2D rootPos = mWorldTransforms[0].position;

    const PoseBlendStack& stack      = mEvaluator.GetBlendStack();
    const int             layerCount = stack.GetLayerCount();

    for (int i = 0; i < layerCount; ++i)
    {
        const Dia::Core::StringCRC layerId  = stack.GetLayerId(i);
        const float                weight   = stack.GetLayerWeight(layerId);
        const int                  priority = stack.GetLayerPriority(layerId);

        const Dia::Maths::Vector2D labelPos(
            rootPos.x + (4.0f * scale),
            rootPos.y + ((-8.0f * i) - 4.0f) * scale);

        char labelText[64];
        std::snprintf(labelText, sizeof(labelText), "%s w=%.2f p=%d", layerId.AsChar(), weight, priority);

        const Dia::Graphics::RGBA colour = (weight > mWeightThreshold)
            ? Dia::Debug::DebugColourPalette::kActive
            : Dia::Debug::DebugColourPalette::kInactive;

        draw.RequestDrawText(labelPos, labelText, fontSize, colour);
    }
}

void AnimBlendWeightsDrawer::DrawImGui()
{
    ImGui::SliderFloat("Weight threshold", &mWeightThreshold, 0.0f, 1.0f);
}

} // namespace Dia::Animation2D

#endif // DIA_DEBUG
