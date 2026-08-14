#include "Animation2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "AnimBlendWeightsDrawer.h"
#include "AnimClipCursorDrawer.h"
#include "AnimSpringDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaAnimation2D/AnimationEvaluator.h>
#include <DiaAnimation2D/AnimClipPlayer.h>
#include <DiaAnimation2D/SpringChain.h>
#include <DiaAnimation2D/AnimClip.h>

namespace Dia::Animation2D
{

namespace
{
    const char* const kDrawerLabels[Animation2DDebugDomain::kDrawerCount] =
    {
        "BlendWeights",
        "ClipCursor",
        "Spring",
    };

    const int kDrawerPriorities[Animation2DDebugDomain::kDrawerCount] =
    {
        40, 41, 42
    };

    const Dia::Core::StringCRC kStageTag("Animation2D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Animation2DDebugDomain::Animation2DDebugDomain(
    const AnimationEvaluator&                                                    evaluator,
    const Dia::Rig2D::Skeleton&                                                  skeleton,
    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, 128>& worldTransforms)
    : mEvaluator(evaluator)
    , mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
{}

Animation2DDebugDomain::~Animation2DDebugDomain() = default;

// Identity
Dia::Core::StringCRC Animation2DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("Animation2D"); }
const char* Animation2DDebugDomain::GetDisplayName() const        { return "Animation2D"; }
const char* Animation2DDebugDomain::GetDescription() const        { return "Animation evaluation — blend weights, clip cursors, spring damping"; }
Dia::Core::StringCRC Animation2DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("Animation"); }
Dia::Core::RGBA Animation2DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kAnimation; }

// Lifecycle
void Animation2DDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mBlendWeightsDrawer = std::make_unique<AnimBlendWeightsDrawer>(mEvaluator, mSkeleton, mWorldTransforms, mgr);
    mClipCursorDrawer   = std::make_unique<AnimClipCursorDrawer>(mEvaluator, mSkeleton, mWorldTransforms, mgr);
    mSpringDrawer       = std::make_unique<AnimSpringDrawer>(mEvaluator, mSkeleton, mWorldTransforms, mgr);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void Animation2DDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mBlendWeightsDrawer.reset(); mClipCursorDrawer.reset(); mSpringDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void Animation2DDebugDomain::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = GetDrawer(i);
        Json::Value entry(Json::objectValue);
        entry["name"]    = kDrawerLabels[i];
        entry["enabled"] = (mLayerManager != nullptr && drawer != nullptr)
                         ? mLayerManager->IsLayerEnabled(drawer->GetLayerName()) : false;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    const Dia::Animation2D::PoseBlendStack& stack = mEvaluator.GetBlendStack();
    const int layerCount = stack.GetLayerCount();

    Json::Value layers(Json::arrayValue);
    for (int i = 0; i < layerCount; ++i)
    {
        const Dia::Core::StringCRC id = stack.GetLayerId(i);
        Json::Value layer(Json::objectValue);
        layer["id"]       = id.AsChar();
        layer["weight"]   = stack.GetLayerWeight(id);
        layer["priority"] = stack.GetLayerPriority(id);
        layers.append(layer);
    }

    const char* activeClipName = "";
    float normalizedTime = 0.0f;
    for (int i = 0; i < layerCount; ++i)
    {
        const Dia::Core::StringCRC id = stack.GetLayerId(i);
        const Dia::Animation2D::AnimClipPlayer* player = mEvaluator.GetClipPlayer(id);
        if (player != nullptr && player->IsPlaying())
        {
            const Dia::Animation2D::AnimClip* clip = player->GetCurrentClip();
            if (clip != nullptr)
            {
                activeClipName = clip->GetId().AsChar();
                normalizedTime = player->GetNormalizedTime();
            }
            break;
        }
    }

    const int sourceCount = mEvaluator.GetSourceCount();
    int springChainCount = 0;
    float maxAngularVelocity = 0.0f;
    for (int i = 0; i < sourceCount; ++i)
    {
        const Dia::Core::StringCRC srcId = mEvaluator.GetSourceId(i);
        const Dia::Animation2D::SpringChain* chain = mEvaluator.GetSpringChain(srcId);
        if (chain == nullptr) continue;
        ++springChainCount;
        const int nodeCount = chain->GetNodeCount();
        for (int n = 0; n < nodeCount; ++n)
        {
            const float av = chain->GetNodeAngularVelocity(n);
            const float absAv = av < 0.0f ? -av : av;
            if (absAv > maxAngularVelocity) maxAngularVelocity = absAv;
        }
    }

    Json::Value springs(Json::objectValue);
    springs["chainCount"]         = springChainCount;
    springs["maxAngularVelocity"] = maxAngularVelocity;

    Json::Value stats(Json::objectValue);
    stats["layerCount"]     = layerCount;
    stats["activeClip"]     = activeClipName;
    stats["normalizedTime"] = normalizedTime;
    stats["layers"]         = layers;
    stats["springs"]        = springs;
    out["stats"] = stats;
}

void Animation2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (mLayerManager == nullptr) return;
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC layerName = ResolveLayerName(args["drawer"].asCString());
        if (layerName == Dia::Core::StringCRC::kZero) return;
        if (mLayerManager->IsLayerEnabled(layerName)) mLayerManager->DisableLayer(layerName);
        else mLayerManager->EnableLayer(layerName);
        return;
    }
    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key = args.isMember("key") && args["key"].isString()
                                       ? Dia::Core::StringCRC(args["key"].asCString())
                                       : kScaleKeyDebugScale;
        if (key == kScaleKeyDebugScale)
            mLayerManager->SetDebugScale(static_cast<float>(args["value"].asDouble()));
    }
}

// Drawer access
int Animation2DDebugDomain::GetDrawerCount() const { return mBlendWeightsDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* Animation2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mBlendWeightsDrawer.get();
    case 1: return mClipCursorDrawer.get();
    case 2: return mSpringDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC Animation2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Animation2DDebugDomain* self = const_cast<Animation2DDebugDomain*>(this);
    const Dia::Core::StringCRC requested(drawerName);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = self->GetDrawer(i);
        if (drawer == nullptr) continue;
        if (requested == Dia::Core::StringCRC(kDrawerLabels[i]) ||
            requested == drawer->GetLayerName())
            return drawer->GetLayerName();
    }
    return Dia::Core::StringCRC::kZero;
}

} // namespace Dia::Animation2D

#endif // DIA_DEBUG
