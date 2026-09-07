#include "Rig2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "BoneLinesDrawer.h"
#include "JointCirclesDrawer.h"
#include "DirectionArrowsDrawer.h"
#include "BoneLabelsDrawer.h"
#include "RestPoseDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

namespace Dia::Rig2D
{

namespace
{
    const char* const kDrawerLabels[Rig2DDebugDomain::kDrawerCount] =
    {
        "BoneLines",
        "Joints",
        "Arrows",
        "Labels",
        "RestPose",
    };

    const int kDrawerPriorities[Rig2DDebugDomain::kDrawerCount] =
    {
        30, 31, 32, 33, 34
    };

    const Dia::Core::StringCRC kStageTag("Rig2D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Rig2DDebugDomain::Rig2DDebugDomain(
    const Skeleton&                                                        skeleton,
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms)
    : mSkeleton(skeleton)
    , mWorldTransforms(worldTransforms)
{}

Rig2DDebugDomain::~Rig2DDebugDomain() = default;

// Identity
Dia::Core::StringCRC Rig2DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("Rig2D"); }
const char* Rig2DDebugDomain::GetDisplayName() const        { return "Rig2D"; }
const char* Rig2DDebugDomain::GetDescription() const        { return "2D skeleton — bones, joints, direction arrows, rest pose"; }
Dia::Core::StringCRC Rig2DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("Animation"); }
Dia::Core::RGBA Rig2DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kAnimation; }

// Lifecycle
void Rig2DDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mBoneLinesDrawer       = std::make_unique<BoneLinesDrawer>(mSkeleton, mWorldTransforms, mgr);
    mJointCirclesDrawer    = std::make_unique<JointCirclesDrawer>(mSkeleton, mWorldTransforms, mgr);
    mDirectionArrowsDrawer = std::make_unique<DirectionArrowsDrawer>(mSkeleton, mWorldTransforms, mgr);
    mBoneLabelsDrawer      = std::make_unique<BoneLabelsDrawer>(mSkeleton, mWorldTransforms, mgr);
    mRestPoseDrawer        = std::make_unique<RestPoseDrawer>(mSkeleton, mWorldTransforms, mgr);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void Rig2DDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mBoneLinesDrawer.reset(); mJointCirclesDrawer.reset(); mDirectionArrowsDrawer.reset();
    mBoneLabelsDrawer.reset(); mRestPoseDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void Rig2DDebugDomain::GetJSONState(Json::Value& out)
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

    Json::Value stats(Json::objectValue);
    stats["boneCount"] = mSkeleton.GetBoneCount();
    out["stats"] = stats;
}

void Rig2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int Rig2DDebugDomain::GetDrawerCount() const { return mBoneLinesDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* Rig2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mBoneLinesDrawer.get();
    case 1: return mJointCirclesDrawer.get();
    case 2: return mDirectionArrowsDrawer.get();
    case 3: return mBoneLabelsDrawer.get();
    case 4: return mRestPoseDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC Rig2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Rig2DDebugDomain* self = const_cast<Rig2DDebugDomain*>(this);
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

} // namespace Dia::Rig2D

#endif // DIA_DEBUG
