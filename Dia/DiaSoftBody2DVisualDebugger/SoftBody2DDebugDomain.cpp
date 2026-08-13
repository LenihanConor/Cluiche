#include "SoftBody2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "SoftParticlesDrawer.h"
#include "SoftConstraintsDrawer.h"
#include "SoftAnchorLinksDrawer.h"
#include "SoftVelocityDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace Dia::SoftBody2D
{

namespace
{
    const char* const kDrawerLabels[SoftBody2DDebugDomain::kDrawerCount] =
    {
        "Particles",
        "Constraints",
        "Anchors",
        "Velocity",
    };

    const int kDrawerPriorities[SoftBody2DDebugDomain::kDrawerCount] =
    {
        20, 21, 22, 23
    };

    const Dia::Core::StringCRC kStageTag("SoftBody2D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

SoftBody2DDebugDomain::SoftBody2DDebugDomain(const SoftBodyWorld& world)
    : mWorld(world)
{}

SoftBody2DDebugDomain::~SoftBody2DDebugDomain() = default;

// Identity
Dia::Core::StringCRC SoftBody2DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("SoftBody2D"); }
const char* SoftBody2DDebugDomain::GetDisplayName() const        { return "SoftBody2D"; }
const char* SoftBody2DDebugDomain::GetDescription() const        { return "Soft body simulation — particles, constraints, anchors, velocities"; }
Dia::Core::StringCRC SoftBody2DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("Physics"); }
Dia::Core::RGBA SoftBody2DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kPhysics; }

// Lifecycle — lazy construction inside Register(); idempotent
void SoftBody2DDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mParticlesDrawer   = std::make_unique<SoftParticlesDrawer>(mWorld, mgr);
    mConstraintsDrawer = std::make_unique<SoftConstraintsDrawer>(mWorld, mgr);
    mAnchorLinksDrawer = std::make_unique<SoftAnchorLinksDrawer>(mWorld, mgr);
    mVelocityDrawer    = std::make_unique<SoftVelocityDrawer>(mWorld, mgr);
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    mLayerManager = &mgr;
}

void SoftBody2DDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (Dia::Debug::IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mParticlesDrawer.reset(); mConstraintsDrawer.reset();
    mAnchorLinksDrawer.reset(); mVelocityDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void SoftBody2DDebugDomain::GetJSONState(Json::Value& out)
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
    out["stats"]   = Json::Value(Json::objectValue);
}

void SoftBody2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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
int SoftBody2DDebugDomain::GetDrawerCount() const { return mParticlesDrawer ? kDrawerCount : 0; }

Dia::Debug::IVisualDebugger* SoftBody2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mParticlesDrawer.get();
    case 1: return mConstraintsDrawer.get();
    case 2: return mAnchorLinksDrawer.get();
    case 3: return mVelocityDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC SoftBody2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    SoftBody2DDebugDomain* self = const_cast<SoftBody2DDebugDomain*>(this);
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

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
