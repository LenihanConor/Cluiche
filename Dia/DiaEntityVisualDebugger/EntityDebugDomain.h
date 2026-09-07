////////////////////////////////////////////////////////////////////////////////
// Filename: EntityDebugDomain.h
// Description: IDebugDomain implementation for the entity/component subsystem.
//              Owns and bulk-registers the six entity debug drawers and bridges
//              their state to DiaDebugPanel.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>

#include <memory>

namespace Dia::Debug  { class DebugLayerManager; }
namespace Dia::Entity { class IEntityInspectable; class Domain; }

namespace Dia::EntityVisualDebugger
{
    class EntityLabelsDrawer;
    class HierarchyLinesDrawer;
    class ComponentFilterHighlightDrawer;
    class EntityPickingDrawer;
    class EntityStatsDrawer;
    class SelectionInspectorDrawer;
}

namespace Dia::EntityVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// EntityDebugDomain
//
// Reference IDebugDomain migration (panel + world drawers). Drawers are owned
// here and created lazily inside Register(), which is where the
// DebugLayerManager (their IDebugContext) becomes available.
//
// Two of the six drawers — Stats and Inspector — emit no world-space
// primitives; they exist purely as panel data sources. They are still reported
// in GetJSONState() and are still toggleable so the panel can hide their
// contribution once Phase 2 moves their text into the "stats" block.
////////////////////////////////////////////////////////////////////////////////
class EntityDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 6;

    EntityDebugDomain(Dia::Entity::IEntityInspectable& inspectable,
                      Dia::Entity::Domain&            domain,
                      Dia::Core::StringCRC            positionComponentTypeId);
    ~EntityDebugDomain() override;

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return true; }

    // ---- IDebugDomain: lifecycle ----
    void Register(Dia::Debug::DebugLayerManager& mgr)   override;
    void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // ---- IDebugDomain: drawer access ----
    int                          GetDrawerCount() const override;
    Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

private:
    Dia::Core::StringCRC ResolveLayerName(const char* drawerName) const;

    Dia::Entity::IEntityInspectable& mInspectable;
    Dia::Entity::Domain&             mDomain;
    Dia::Core::StringCRC             mPositionTypeId;
    Dia::Debug::DebugLayerManager*   mLayerManager = nullptr;

    std::unique_ptr<EntityLabelsDrawer>             mLabelsDrawer;
    std::unique_ptr<HierarchyLinesDrawer>           mHierarchyDrawer;
    std::unique_ptr<ComponentFilterHighlightDrawer> mHighlightDrawer;
    std::unique_ptr<EntityPickingDrawer>            mPickingDrawer;
    std::unique_ptr<EntityStatsDrawer>              mStatsDrawer;
    std::unique_ptr<SelectionInspectorDrawer>       mInspectorDrawer;
};

} // namespace Dia::EntityVisualDebugger

#endif // DIA_DEBUG
