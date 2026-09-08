#include "IK2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "IKChainBonesDrawer.h"
#include "IKChainJointsDrawer.h"
#include "IKChainArrowsDrawer.h"
#include "IKReachCirclesDrawer.h"

#include <DiaDebugDraw/Domain/IDebugLayerRegistry.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaIK2D/IKSolver.h>

namespace Dia::IK2D
{

namespace
{
    const char* const kDrawerLabels[IK2DDebugDomain::kDrawerCount] =
    {
        "ChainBones",
        "ChainJoints",
        "ChainArrows",
        "ReachCircles",
    };

    const int kDrawerPriorities[IK2DDebugDomain::kDrawerCount] =
    {
        35, 36, 37, 38
    };

    const Dia::Core::StringCRC kStageTag("IK2D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

IK2DDebugDomain::IK2DDebugDomain(const IKSolver&            solver,
                                  const Dia::Rig2D::Skeleton& skeleton)
    : mSolver(solver)
    , mSkeleton(skeleton)
{}

IK2DDebugDomain::~IK2DDebugDomain() = default;

// Identity
Dia::Core::StringCRC IK2DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("IK2D"); }
const char* IK2DDebugDomain::GetDisplayName() const        { return "IK2D"; }
const char* IK2DDebugDomain::GetDescription() const        { return "Inverse kinematics — chain bones, joints, arrows, reach circles"; }
Dia::Core::StringCRC IK2DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("Animation"); }
Dia::Core::RGBA IK2DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kAnimation; }

// Lifecycle
void IK2DDebugDomain::Register(Dia::Debug::IDebugLayerRegistry& mgr)
{
    if (mLayerManager != nullptr) return;
    mChainBonesDrawer   = std::make_unique<IKChainBonesDrawer>(mSolver, mSkeleton, mgr);
    mChainJointsDrawer  = std::make_unique<IKChainJointsDrawer>(mSolver, mSkeleton, mgr);
    mChainArrowsDrawer  = std::make_unique<IKChainArrowsDrawer>(mSolver, mSkeleton, mgr);
    mReachCirclesDrawer = std::make_unique<IKReachCirclesDrawer>(mSolver, mSkeleton, mgr);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void IK2DDebugDomain::Unregister(Dia::Debug::IDebugLayerRegistry& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mChainBonesDrawer.reset(); mChainJointsDrawer.reset();
    mChainArrowsDrawer.reset(); mReachCirclesDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void IK2DDebugDomain::GetJSONState(Json::Value& out)
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

    const int chainCount = mSolver.GetChainCount();
    Json::Value chains(Json::arrayValue);
    for (int i = 0; i < chainCount; ++i)
    {
        Json::Value c(Json::objectValue);
        c["index"]              = i;
        c["id"]                 = mSolver.GetChainId(i).AsChar();
        c["solved"]             = mSolver.IsSolved(i);
        c["iterations"]         = mSolver.GetLastIterationCount(i);
        c["endEffectorError"]   = mSolver.GetEndEffectorError(i);
        chains.append(c);
    }

    Json::Value stats(Json::objectValue);
    stats["chainCount"] = chainCount;
    stats["chains"]     = chains;
    out["stats"] = stats;
}

void IK2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int IK2DDebugDomain::GetDrawerCount() const { return mChainBonesDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* IK2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mChainBonesDrawer.get();
    case 1: return mChainJointsDrawer.get();
    case 2: return mChainArrowsDrawer.get();
    case 3: return mReachCirclesDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC IK2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    IK2DDebugDomain* self = const_cast<IK2DDebugDomain*>(this);
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

} // namespace Dia::IK2D

#endif // DIA_DEBUG
