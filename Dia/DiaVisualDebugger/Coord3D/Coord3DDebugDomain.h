////////////////////////////////////////////////////////////////////////////////
// Filename: Coord3DDebugDomain.h
// Description: IDebugDomain for the 3D coordinate system debug overlay. Owns
//              and bulk-registers all four Coord3D drawers via
//              RegisterWithoutDraw (3D drawers are driven by DrawCoord3D, not
//              the 2D layer manager draw loop). Drawers start disabled.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug
{
    class DebugLayerManager;
    class Coord3DOriginDrawer;
    class Coord3DAxesDrawer;
    class Coord3DGridDrawer;
    class Coord3DCameraDrawer;
}

namespace Dia::Graphics3D { struct FrameData3D; }

namespace Dia::Debug
{

////////////////////////////////////////////////////////////////////////////////
// Coord3DDebugDomain
//
// No constructor args — drawers use the DebugLayerManager itself as their
// IDebugContext, supplied at Register() time. Drawers are registered via
// RegisterWithoutDraw() since they target Dia::Graphics3D::FrameData3D, not
// the 2D FrameData consumed by the layer manager's Draw() loop. All four layers
// are disabled after registration (opt-in overlay).
////////////////////////////////////////////////////////////////////////////////
class Coord3DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 4;

    Coord3DDebugDomain();
    ~Coord3DDebugDomain() override;

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

    // ---- 3D draw pass (driven externally by VisualDebuggerModule::DrawCoord3D) ----
    void DrawCoord3D(Dia::Graphics3D::FrameData3D& frame);

private:
    Dia::Core::StringCRC ResolveLayerName(const char* drawerName) const;

    DebugLayerManager* mLayerManager = nullptr;

    std::unique_ptr<Coord3DOriginDrawer>  mOriginDrawer;
    std::unique_ptr<Coord3DAxesDrawer>    mAxesDrawer;
    std::unique_ptr<Coord3DGridDrawer>    mGridDrawer;
    std::unique_ptr<Coord3DCameraDrawer>  mCameraDrawer;
};

} // namespace Dia::Debug

#endif // DIA_DEBUG
