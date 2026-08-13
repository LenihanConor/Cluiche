////////////////////////////////////////////////////////////////////////////////
// Filename: Scene2DDebugDomain.h
// Description: IDebugDomain for the 2D scene visual debugger subsystem. Owns
//              and bulk-registers SceneOverviewDrawer.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug    { class DebugLayerManager; }
namespace Dia::Camera2D  { class CameraRegistry2D; }
namespace Dia::Lighting2D { class LightRegistry2D; }
namespace Dia::Scene2D   { class LayerTable; }

namespace Dia::Scene2DVisualDebugger { class SceneOverviewDrawer; }

namespace Dia::Scene2DVisualDebugger
{

////////////////////////////////////////////////////////////////////////////////
// Scene2DDebugDomain
//
// Constructor stores const refs to the three scene registries. SceneOverviewDrawer
// is created lazily inside Register() once the DebugLayerManager is available.
////////////////////////////////////////////////////////////////////////////////
class Scene2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 1;

    Scene2DDebugDomain(const Dia::Camera2D::CameraRegistry2D&  cameraRegistry,
                       const Dia::Lighting2D::LightRegistry2D& lightRegistry,
                       const Dia::Scene2D::LayerTable&         layerTable);
    ~Scene2DDebugDomain() override;

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

    const Dia::Camera2D::CameraRegistry2D&  mCameraRegistry;
    const Dia::Lighting2D::LightRegistry2D& mLightRegistry;
    const Dia::Scene2D::LayerTable&         mLayerTable;
    Dia::Debug::DebugLayerManager*          mLayerManager = nullptr;

    std::unique_ptr<SceneOverviewDrawer> mOverviewDrawer;
};

} // namespace Dia::Scene2DVisualDebugger

#endif // DIA_DEBUG
