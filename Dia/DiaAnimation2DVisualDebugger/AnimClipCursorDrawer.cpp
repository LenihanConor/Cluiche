////////////////////////////////////////////////////////////////////////////////
// Filename: AnimClipCursorDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaAnimation2DVisualDebugger/AnimClipCursorDrawer.h"

#ifdef DIA_DEBUG

#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>

#include <algorithm>
#include <cstdio>

namespace Dia { namespace Animation2D {

AnimClipCursorDrawer::AnimClipCursorDrawer(
    const AnimationEvaluator&                                                    evaluator,
    const Dia::Rig2D::Skeleton&                                                  skeleton,
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms,
    const Dia::Debug::DebugLayerManager&                                         manager)
    : mEvaluator(evaluator)
    , mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
    , mManager(manager)
{}

Dia::Core::StringCRC AnimClipCursorDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kAnimClipCursor;
}

void AnimClipCursorDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    if (mWorldTransforms.Size() == 0) return;

    const float scale    = mManager.GetDebugScale();
    const float fontSize = std::min(10.0f * scale, kMaxFontSize);

    const Dia::Maths::Vector2D rootPos = mWorldTransforms[0].position;

    const int sourceCount = mEvaluator.GetSourceCount();
    for (int i = 0; i < sourceCount; ++i)
    {
        const Dia::Core::StringCRC id = mEvaluator.GetSourceId(i);
        const AnimClipPlayer* player  = mEvaluator.GetClipPlayer(id);
        if (player == nullptr) continue;  // source is a spring chain

        const Dia::Maths::Vector2D labelPos(
            rootPos.x + (-20.0f * scale),
            rootPos.y + ((-8.0f * i) - 4.0f) * scale);

        char labelText[64];
        Dia::Graphics::RGBA colour;

        if (player->IsPlaying())
        {
            const float normTime = player->GetNormalizedTime();
            std::snprintf(labelText, sizeof(labelText), "[%s] %.2f", id.AsChar(), normTime);
            colour = Dia::Debug::DebugColourPalette::kActive;
        }
        else
        {
            std::snprintf(labelText, sizeof(labelText), "[%s] stopped", id.AsChar());
            colour = Dia::Debug::DebugColourPalette::kInactive;
        }

        frameData.RequestDrawText(labelPos, labelText, fontSize, colour);
    }
}

} } // namespace Dia::Animation2D

#endif // DIA_DEBUG
