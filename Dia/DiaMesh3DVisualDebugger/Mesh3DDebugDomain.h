////////////////////////////////////////////////////////////////////////////////
// Filename: Mesh3DDebugDomain.h
// Description: IDebugDomain for the Mesh3D visual debugger subsystem. Owns and
//              bulk-registers MeshBoundsDrawer, MeshOriginDrawer, and
//              MeshStatsDrawer.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug { class DebugLayerManager; }
namespace Dia { namespace Graphics3D { class Mesh3DFrameData; } }

namespace Dia { namespace Mesh3D {
    class Mesh3DAssetHandler;
    class MeshBoundsDrawer;
    class MeshOriginDrawer;
    class MeshStatsDrawer;
} }

namespace Dia { namespace Mesh3D {

////////////////////////////////////////////////////////////////////////////////
// Mesh3DDebugDomain
//
// Constructor takes frame data and asset handler by const ref (both must
// outlive the domain). Drawers are created lazily in Register().
////////////////////////////////////////////////////////////////////////////////
class Mesh3DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 3;

    Mesh3DDebugDomain(const Dia::Graphics3D::Mesh3DFrameData& frameData,
                      const Dia::Mesh3D::Mesh3DAssetHandler&  assetHandler);
    ~Mesh3DDebugDomain() override;

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

    const Dia::Graphics3D::Mesh3DFrameData& mFrameData;
    const Dia::Mesh3D::Mesh3DAssetHandler&  mAssetHandler;
    Dia::Debug::DebugLayerManager*          mLayerManager = nullptr;

    std::unique_ptr<MeshBoundsDrawer>  mBoundsDrawer;
    std::unique_ptr<MeshOriginDrawer>  mOriginsDrawer;
    std::unique_ptr<MeshStatsDrawer>   mStatsDrawer;
};

} } // namespace Dia::Mesh3D

#endif // DIA_DEBUG
