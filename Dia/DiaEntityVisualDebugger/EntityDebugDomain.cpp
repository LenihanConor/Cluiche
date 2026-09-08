#include "EntityDebugDomain.h"

#ifdef DIA_DEBUG

#include "EntityLabelsDrawer.h"
#include "HierarchyLinesDrawer.h"
#include "ComponentFilterHighlightDrawer.h"
#include "EntityPickingDrawer.h"
#include "EntityStatsDrawer.h"
#include "SelectionInspectorDrawer.h"

#include <DiaDebugDraw/Domain/IDebugLayerRegistry.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaEntity/IEntityInspectable.h>

namespace Dia::EntityVisualDebugger
{

namespace
{
    // Panel-facing labels, in registration order. Index must line up with
    // GetDrawer() and kDrawerPriorities.
    const char* const kDrawerLabels[EntityDebugDomain::kDrawerCount] =
    {
        "Labels",
        "Hierarchy",
        "Highlight",
        "Picking",
        "Stats",
        "Inspector",
    };

    // Priorities preserved from the pre-migration EntityVisualDebuggerModule.
    const int kDrawerPriorities[EntityDebugDomain::kDrawerCount] =
    {
        55, 56, 57, 58, 59, 60
    };

    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

EntityDebugDomain::EntityDebugDomain(Dia::Entity::IEntityInspectable& inspectable,
                                     Dia::Entity::Domain&            domain,
                                     Dia::Core::StringCRC            positionComponentTypeId)
    : mInspectable(inspectable)
    , mDomain(domain)
    , mPositionTypeId(positionComponentTypeId)
{
}

EntityDebugDomain::~EntityDebugDomain() = default;

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

Dia::Core::StringCRC EntityDebugDomain::GetDomainId() const
{
    return Dia::Core::StringCRC("Entity");
}

const char* EntityDebugDomain::GetDisplayName() const
{
    return "Entity";
}

const char* EntityDebugDomain::GetDescription() const
{
    // AC-5: must stay ≤80 characters.
    return "Entity graph - labels, hierarchy, picking, component inspector";
}

Dia::Core::StringCRC EntityDebugDomain::GetGroup() const
{
    return Dia::Core::StringCRC("Entity");
}

Dia::Core::RGBA EntityDebugDomain::GetAccentColour() const
{
    return Dia::VisualDebugger::DebugGroupAccents::kEntity;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void EntityDebugDomain::Register(Dia::Debug::IDebugLayerRegistry& mgr)
{
    if (mLayerManager != nullptr)
        return;   // already registered — idempotent

    // DebugLayerManager IS an IDebugContext, so it doubles as the drawers'
    // context (global debug scale, selected entity).
    mLabelsDrawer    = std::make_unique<EntityLabelsDrawer>(mInspectable, mDomain, mgr, mPositionTypeId);
    mHierarchyDrawer = std::make_unique<HierarchyLinesDrawer>(mInspectable, mDomain, mgr, mPositionTypeId);
    mHighlightDrawer = std::make_unique<ComponentFilterHighlightDrawer>(mInspectable, mDomain, mgr, mPositionTypeId);
    mPickingDrawer   = std::make_unique<EntityPickingDrawer>(mInspectable, mDomain, mgr, mPositionTypeId);
    mStatsDrawer     = std::make_unique<EntityStatsDrawer>(mInspectable);
    mInspectorDrawer = std::make_unique<SelectionInspectorDrawer>(mInspectable, mDomain, mgr);

    for (int i = 0; i < kDrawerCount; ++i)
    {
        mgr.Register(GetDrawer(i), kDrawerPriorities[i], Dia::Debug::LayerNames::kEntityStageTag);
    }

    mLayerManager = &mgr;
}

void EntityDebugDomain::Unregister(Dia::Debug::IDebugLayerRegistry& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
    {
        if (Dia::Debug::IVisualDebugger* drawer = GetDrawer(i))
            mgr.Unregister(drawer->GetLayerName());
    }

    mLabelsDrawer.reset();
    mHierarchyDrawer.reset();
    mHighlightDrawer.reset();
    mPickingDrawer.reset();
    mStatsDrawer.reset();
    mInspectorDrawer.reset();

    mLayerManager = nullptr;
}

// ---------------------------------------------------------------------------
// Panel bridge
// ---------------------------------------------------------------------------

void EntityDebugDomain::GetJSONState(Json::Value& out)
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

    Json::Value stats(Json::objectValue);
    stats["alive"]      = static_cast<int>(mInspectable.GetEntityCount());
    stats["queryCount"] = static_cast<int>(mInspectable.GetQueryCount());
    if (mLayerManager != nullptr)
    {
        const uint32_t selId = mLayerManager->GetSelectedEntityId();
        Json::Value sel(Json::objectValue);
        sel["id"]      = static_cast<int>(selId);
        sel["hasSelection"] = (selId != 0);
        stats["selection"] = sel;
    }
    out["stats"] = stats;
}

void EntityDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
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

        // No domain-local scale parameters yet — the only tunable is the shared
        // debug scale that the label font size and highlight radii read.
        const Dia::Core::StringCRC key = args.isMember("key") && args["key"].isString()
                                       ? Dia::Core::StringCRC(args["key"].asCString())
                                       : kScaleKeyDebugScale;
        if (key == kScaleKeyDebugScale)
            mLayerManager->SetDebugScale(static_cast<float>(args["value"].asDouble()));
    }
}

// ---------------------------------------------------------------------------
// Drawer access
// ---------------------------------------------------------------------------

int EntityDebugDomain::GetDrawerCount() const
{
    return mLabelsDrawer ? kDrawerCount : 0;
}

Dia::Debug::IVisualDebugger* EntityDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mLabelsDrawer.get();
    case 1: return mHierarchyDrawer.get();
    case 2: return mHighlightDrawer.get();
    case 3: return mPickingDrawer.get();
    case 4: return mStatsDrawer.get();
    case 5: return mInspectorDrawer.get();
    default: return nullptr;
    }
}

Dia::Core::StringCRC EntityDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0')
        return Dia::Core::StringCRC::kZero;

    EntityDebugDomain* self = const_cast<EntityDebugDomain*>(this);
    const Dia::Core::StringCRC requested(drawerName);

    for (int i = 0; i < kDrawerCount; ++i)
    {
        const Dia::Debug::IVisualDebugger* drawer = self->GetDrawer(i);
        if (drawer == nullptr)
            continue;

        // Panel label ("Labels") or the raw layer key ("entity.labels").
        if (requested == Dia::Core::StringCRC(kDrawerLabels[i]) ||
            requested == drawer->GetLayerName())
        {
            return drawer->GetLayerName();
        }
    }

    return Dia::Core::StringCRC::kZero;
}

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
