////////////////////////////////////////////////////////////////////////////////
// Filename: Coord2DDebugDomain.h
// Description: IDebugDomain for the 2D coordinate system debug overlay. Owns
//              and bulk-registers all five Coord2D drawers. Drawers start
//              disabled — the overlay is opt-in via the debug panel or DiaAPI.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug
{
    class DebugLayerManager;
    class Coord2DOriginDrawer;
    class Coord2DAxesDrawer;
    class Coord2DGridDrawer;
    class Coord2DBoundsDrawer;
    class Coord2DCursorDrawer;
}

namespace Dia::Debug
{

////////////////////////////////////////////////////////////////////////////////
// Coord2DDebugDomain
//
// No constructor args — drawers use the DebugLayerManager itself as their
// IDebugContext, supplied at Register() time. All five layers are disabled
// after registration (opt-in overlay); enable via panel toggle or DiaAPI.
////////////////////////////////////////////////////////////////////////////////
class Coord2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 5;

    Coord2DDebugDomain();
    ~Coord2DDebugDomain() override;

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

    DebugLayerManager* mLayerManager = nullptr;

    std::unique_ptr<Coord2DOriginDrawer>  mOriginDrawer;
    std::unique_ptr<Coord2DAxesDrawer>    mAxesDrawer;
    std::unique_ptr<Coord2DGridDrawer>    mGridDrawer;
    std::unique_ptr<Coord2DBoundsDrawer>  mBoundsDrawer;
    std::unique_ptr<Coord2DCursorDrawer>  mCursorDrawer;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
