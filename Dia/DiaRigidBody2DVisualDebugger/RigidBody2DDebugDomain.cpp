#include "RigidBody2DDebugDomain.h"

#ifdef DIA_DEBUG

#include "PhysicsShapesDrawer.h"
#include "VelocityArrowsDrawer.h"
#include "ContactNormalsDrawer.h"
#include "PhysicsAABBDrawer.h"
#include "ConstraintLinesDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/Body2DBase.h>

namespace Dia::RigidBody2D
{

namespace
{
    // Panel-facing labels, in registration order. Index must line up with
    // GetDrawer() / GetJSONState() / the layer-name table below.
    const char* const kDrawerLabels[RigidBody2DDebugDomain::kDrawerCount] =
    {
        "Shapes",
        "Velocity",
        "Contacts",
        "AABB",
        "Constraints",
    };

    // Draw priorities preserved from the pre-migration Physics2DModule wiring:
    // lower value drawn first (underneath).
    const int kDrawerPriorities[RigidBody2DDebugDomain::kDrawerCount] =
    {
        10, 11, 12, 13, 14
    };

    const Dia::Core::StringCRC kStageTag("RigidBody2D");
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

RigidBody2DDebugDomain::RigidBody2DDebugDomain(const PhysicsWorld& world)
    : mWorld(world)
{
}

RigidBody2DDebugDomain::~RigidBody2DDebugDomain() = default;

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

Dia::Core::StringCRC RigidBody2DDebugDomain::GetDomainId() const
{
    return Dia::Core::StringCRC("RigidBody2D");
}

const char* RigidBody2DDebugDomain::GetDisplayName() const
{
    return "RigidBody2D";
}

const char* RigidBody2DDebugDomain::GetDescription() const
{
    // AC-5: must stay ≤80 characters.
    return "Rigid body - shapes, contacts, constraints, velocities";
}

Dia::Core::StringCRC RigidBody2DDebugDomain::GetGroup() const
{
    return Dia::Core::StringCRC("Physics");
}

Dia::Core::RGBA RigidBody2DDebugDomain::GetAccentColour() const
{
    return Dia::VisualDebugger::DebugGroupAccents::kPhysics;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void RigidBody2DDebugDomain::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr)
        return;   // already registered — idempotent

    // DebugLayerManager IS an IDebugContext, so it doubles as the drawers'
    // context (global debug scale, selection, viewport).
    mShapesDrawer      = std::make_unique<PhysicsShapesDrawer>(mWorld, mgr);
    mVelocityDrawer    = std::make_unique<VelocityArrowsDrawer>(mWorld, mgr);
    mContactsDrawer    = std::make_unique<ContactNormalsDrawer>(mWorld, mgr);
    mAABBDrawer        = std::make_unique<PhysicsAABBDrawer>(mWorld, mgr);
    mConstraintsDrawer = std::make_unique<ConstraintLinesDrawer>(mWorld, mgr);

    for (int i = 0; i < kDrawerCount; ++i)
    {
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], kStageTag);
    }

    mLayerManager = &mgr;
}

void RigidBody2DDebugDomain::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
    {
        if (Dia::Debug::IVisualDebugger* drawer = GetDrawer(i))
            mgr.Unregister(drawer->GetLayerName());
    }

    mShapesDrawer.reset();
    mVelocityDrawer.reset();
    mContactsDrawer.reset();
    mAABBDrawer.reset();
    mConstraintsDrawer.reset();

    mLayerManager = nullptr;
}

// ---------------------------------------------------------------------------
// Panel bridge
// ---------------------------------------------------------------------------

void RigidBody2DDebugDomain::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);

    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = GetDrawer(i);

        Json::Value entry(Json::objectValue);
        entry["name"]    = kDrawerLabels[i];
        entry["enabled"] = (mLayerManager != nullptr && drawer != nullptr)
                         ? mLayerManager->IsLayerEnabled(drawer->GetLayerName())
                         : false;
        drawers.append(entry);
    }

    out["drawers"] = drawers;

    // Body counts — iterate rigid bodies to classify awake/sleeping/static.
    int active = 0, sleeping = 0, staticCount = 0;
    const auto& rigidBodies = mWorld.GetRigidBodies();
    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
    {
        const Dia::RigidBody2D::RigidBody2D* b = rigidBodies[i];
        if (b == nullptr) continue;
        if (b->GetBodyType() == Dia::RigidBody2D::BodyType::kStatic)
            ++staticCount;
        else if (b->IsAwake())
            ++active;
        else
            ++sleeping;
    }
    const int contacts    = static_cast<int>(mWorld.GetLastContacts().Size());
    const int constraints = static_cast<int>(mWorld.GetConstraints().Size());

    Json::Value stats(Json::objectValue);
    stats["active"]    = active;
    stats["sleeping"]  = sleeping;
    stats["static"]    = staticCount;
    stats["total"]     = active + sleeping + staticCount;
    stats["contacts"]  = contacts;
    stats["constraints"] = constraints;

    Json::Value params(Json::objectValue);
    params["velocityScale"] = mParamVelocityScale;
    params["normalLength"]  = mParamNormalLength;

    out["stats"]  = stats;
    out["params"] = params;
}

void RigidBody2DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (mLayerManager == nullptr)
        return;

    // OnCommand arrives on the Sim PU (already drained through
    // VisualDebuggerModule's command queue), so direct manager calls are safe.
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString())
            return;

        const Dia::Core::StringCRC layerName = ResolveLayerName(args["drawer"].asCString());
        if (layerName == Dia::Core::StringCRC::kZero)
            return;

        if (mLayerManager->IsLayerEnabled(layerName))
            mLayerManager->DisableLayer(layerName);
        else
            mLayerManager->EnableLayer(layerName);

        return;
    }

    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("value") || !args["value"].isNumeric())
            return;

        const Dia::Core::StringCRC key = args.isMember("key") && args["key"].isString()
                                       ? Dia::Core::StringCRC(args["key"].asCString())
                                       : kScaleKeyDebugScale;
        const float val = static_cast<float>(args["value"].asDouble());
        if (key == kScaleKeyDebugScale)
            mLayerManager->SetDebugScale(val);
        else if (key == Dia::Core::StringCRC("velocityScale"))
            mParamVelocityScale = val;
        else if (key == Dia::Core::StringCRC("normalLength"))
            mParamNormalLength = val;
    }
}

// ---------------------------------------------------------------------------
// Drawer access
// ---------------------------------------------------------------------------

int RigidBody2DDebugDomain::GetDrawerCount() const
{
    return mShapesDrawer ? kDrawerCount : 0;
}

Dia::Debug::IVisualDebugger* RigidBody2DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mShapesDrawer.get();
    case 1: return mVelocityDrawer.get();
    case 2: return mContactsDrawer.get();
    case 3: return mAABBDrawer.get();
    case 4: return mConstraintsDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC RigidBody2DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0')
        return Dia::Core::StringCRC::kZero;

    RigidBody2DDebugDomain* self = const_cast<RigidBody2DDebugDomain*>(this);
    const Dia::Core::StringCRC requested(drawerName);

    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = self->GetDrawer(i);
        if (drawer == nullptr)
            continue;

        // Panel label ("Shapes") or the raw layer key ("physics.shapes").
        if (requested == Dia::Core::StringCRC(kDrawerLabels[i]) ||
            requested == drawer->GetLayerName())
        {
            return drawer->GetLayerName();
        }
    }

    return Dia::Core::StringCRC::kZero;
}

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
