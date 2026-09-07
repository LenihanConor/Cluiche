////////////////////////////////////////////////////////////////////////////////
// Filename: Geometry2DDebugDomain.h
// Description: IDebugDomain for the 2D geometry visual debugger subsystem.
//              Owns and bulk-registers the three non-template geometry drawers
//              (ShapeDrawer, ShapeLabelsDrawer, AABBOverlayDrawer). The four
//              template drawers (BVH, Quadtree, SpatialGrid, HexGrid) remain
//              caller-registered.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::Geometry2DVisualDebugger
{
    class ShapeDrawer;
    class ShapeLabelsDrawer;
    class AABBOverlayDrawer;
}

namespace Dia::Geometry2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// Geometry2DDebugDomain
//
// Drawers are submit-per-frame: Draw() emits nothing unless the caller submits
// shapes via the drawer's Submit*() methods each frame. Drawers are owned here
// and created lazily inside Register() — they need the DebugLayerManager as
// their IDebugContext.
////////////////////////////////////////////////////////////////////////////////
class Geometry2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 3;

    Geometry2DDebugDomain();
    ~Geometry2DDebugDomain() override;

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

    Dia::Debug::DebugLayerManager* mLayerManager = nullptr;

    std::unique_ptr<ShapeDrawer>       mShapesDrawer;
    std::unique_ptr<ShapeLabelsDrawer> mLabelsDrawer;
    std::unique_ptr<AABBOverlayDrawer> mAABBDrawer;
};

} // namespace Dia::Geometry2DVisualDebugger

#endif // DIA_DEBUG
