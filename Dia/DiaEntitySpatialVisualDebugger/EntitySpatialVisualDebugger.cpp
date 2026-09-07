////////////////////////////////////////////////////////////////////////////////
// Filename: EntitySpatialVisualDebugger.cpp
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#include "EntitySpatialVisualDebugger.h"
#ifdef DIA_DEBUG

#include <DiaEntitySpatial/Adaptors/EntitySpatialOverlay.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaEntity/Domain.h>
#include <memory>

namespace
{
    const Dia::Core::StringCRC kCmdToggle     ("toggle");
    const Dia::Core::StringCRC kCmdSetScale   ("setScale");
    const Dia::Core::StringCRC kDrawerGrid    ("Grid");
    const Dia::Core::StringCRC kDrawerEntities("Entities");
    const Dia::Core::StringCRC kDrawerQuery   ("Query");
    const Dia::Core::StringCRC kScaleLabelSize("labelSize");
    const Dia::Core::StringCRC kLayerGrid     ("entityspatial.grid");
    const Dia::Core::StringCRC kLayerEntities ("entityspatial.entities");
    const Dia::Core::StringCRC kLayerQuery    ("entityspatial.query");
}

namespace Dia { namespace EntitySpatial {

EntitySpatialVisualDebugger::EntitySpatialVisualDebugger(
    const EntitySpatialModule&           module,
    const Dia::Entity::Domain&           domain,
    const EntitySpatialIndex::SquareDef& def)
    : mModule(module), mDomain(domain), mDef(def)
{}

EntitySpatialVisualDebugger::~EntitySpatialVisualDebugger() = default;

Dia::Core::StringCRC EntitySpatialVisualDebugger::GetDomainId()     const { return Dia::Core::StringCRC("entityspatial"); }
const char*          EntitySpatialVisualDebugger::GetDisplayName()  const { return "Entity Spatial"; }
const char*          EntitySpatialVisualDebugger::GetDescription()  const { return "Spatial index \xe2\x80\x94 grid cells, entity positions, query shapes"; }
Dia::Core::StringCRC EntitySpatialVisualDebugger::GetGroup()        const { return Dia::Core::StringCRC("Entity"); }
Dia::Core::RGBA      EntitySpatialVisualDebugger::GetAccentColour() const { return Dia::VisualDebugger::DebugGroupAccents::kEntity; }

void EntitySpatialVisualDebugger::Register(Dia::Debug::DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;
    mGrid     = std::make_unique<Adaptors::EntitySpatialGridOverlay>(mDef, kLayerGrid);
    mEntities = std::make_unique<Adaptors::EntitySpatialEntityOverlay>(mModule, mDomain, kLayerEntities);
    mQuery    = std::make_unique<Adaptors::EntitySpatialQueryOverlay>(kLayerQuery);
    mGrid->SetEnabled(mGridEnabled.load());
    mEntities->SetEnabled(mEntitiesEnabled.load());
    mEntities->SetLabelSize(mLabelSize);
    mQuery->SetEnabled(mQueryEnabled.load());
    for (int i = 0; i < 3; ++i)
        mgr.Register(GetDrawer(i), 40 + i, Dia::Core::StringCRC("EntitySpatial"));
    mLayerManager = &mgr;
}

void EntitySpatialVisualDebugger::Unregister(Dia::Debug::DebugLayerManager& mgr)
{
    for (int i = 0; i < 3; ++i)
        if (auto* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mGrid.reset();
    mEntities.reset();
    mQuery.reset();
    mLayerManager = nullptr;
}

Dia::Debug::IVisualDebugger* EntitySpatialVisualDebugger::GetDrawer(int index)
{
    switch (index)
    {
        case 0: return mGrid.get();
        case 1: return mEntities.get();
        case 2: return mQuery.get();
        default: return nullptr;
    }
}

void EntitySpatialVisualDebugger::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value e(Json::objectValue);
        e["name"] = "Grid"; e["enabled"] = mGridEnabled.load();
        drawers.append(e);
    }
    {
        Json::Value e(Json::objectValue);
        e["name"] = "Entities"; e["enabled"] = mEntitiesEnabled.load();
        drawers.append(e);
    }
    {
        Json::Value e(Json::objectValue);
        e["name"] = "Query"; e["enabled"] = mQueryEnabled.load();
        drawers.append(e);
    }
    out["drawers"] = drawers;

    Json::Value stats(Json::objectValue);
    stats["labelSize"] = mLabelSize;
    out["stats"] = stats;
}

void EntitySpatialVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC name(args["drawer"].asCString());

        if (name == kDrawerGrid)
        {
            const bool v = !mGridEnabled.load();
            mGridEnabled.store(v);
            if (mGrid) mGrid->SetEnabled(v);
        }
        else if (name == kDrawerEntities)
        {
            const bool v = !mEntitiesEnabled.load();
            mEntitiesEnabled.store(v);
            if (mEntities) mEntities->SetEnabled(v);
        }
        else if (name == kDrawerQuery)
        {
            const bool v = !mQueryEnabled.load();
            mQueryEnabled.store(v);
            if (mQuery) mQuery->SetEnabled(v);
        }
        return;
    }

    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("key")   || !args["key"].isString())    return;
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key(args["key"].asCString());
        if (key == kScaleLabelSize)
        {
            mLabelSize = static_cast<float>(args["value"].asDouble());
            if (mEntities) mEntities->SetLabelSize(mLabelSize);
        }
        // other setScale keys are no-op
        return;
    }
    // unknown command — no-op, no crash
}

} } // namespace Dia::EntitySpatial

#endif // DIA_DEBUG
