////////////////////////////////////////////////////////////////////////////////
// Filename: Lighting3DDebugDomain.h
// Description: IDebugDomain for the 3D lighting visual debugger subsystem. Owns
//              and bulk-registers LightWidgetsDrawer and LightPathArcDrawer.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug { class IDebugLayerRegistry; }

namespace Dia { namespace Lighting3D {
    class LightRegistry3D;
    class LightWidgetsDrawer;
    class LightPathArcDrawer;
    class LightRangesDrawer;
} }

namespace Dia { namespace Lighting3D {

////////////////////////////////////////////////////////////////////////////////
// Lighting3DDebugDomain
//
// Constructor takes a const ref to the LightRegistry3D (must outlive the
// domain). Drawers are created lazily inside Register().
////////////////////////////////////////////////////////////////////////////////
class Lighting3DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 3;

    explicit Lighting3DDebugDomain(const LightRegistry3D& registry);
    ~Lighting3DDebugDomain() override;

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return true; }

    // ---- IDebugDomain: lifecycle ----
    void Register(Dia::Debug::IDebugLayerRegistry& mgr)   override;
    void Unregister(Dia::Debug::IDebugLayerRegistry& mgr) override;

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // ---- IDebugDomain: drawer access ----
    int                          GetDrawerCount() const override;
    Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

private:
    Dia::Core::StringCRC ResolveLayerName(const char* drawerName) const;

    const LightRegistry3D&         mRegistry;
    Dia::Debug::IDebugLayerRegistry* mLayerManager = nullptr;

    std::unique_ptr<LightWidgetsDrawer>  mWidgetsDrawer;
    std::unique_ptr<LightPathArcDrawer>  mPathArcDrawer;
    std::unique_ptr<LightRangesDrawer>   mRangesDrawer;
};

} } // namespace Dia::Lighting3D

#endif // DIA_DEBUG
