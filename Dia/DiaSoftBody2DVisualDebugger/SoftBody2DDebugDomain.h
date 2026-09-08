////////////////////////////////////////////////////////////////////////////////
// Filename: SoftBody2DDebugDomain.h
// Description: IDebugDomain implementation for the 2D soft-body physics
//              subsystem. Owns and bulk-registers the four world-space
//              soft-body drawers and bridges their state to DiaDebugPanel.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug { class IDebugLayerRegistry; }

namespace Dia::SoftBody2D
{
    class SoftBodyWorld;
    class SoftParticlesDrawer;
    class SoftConstraintsDrawer;
    class SoftAnchorLinksDrawer;
    class SoftVelocityDrawer;
}

namespace Dia::SoftBody2D
{

class SoftBody2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 4;

    explicit SoftBody2DDebugDomain(const SoftBodyWorld& world);
    ~SoftBody2DDebugDomain() override;

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

    const SoftBodyWorld&           mWorld;
    Dia::Debug::IDebugLayerRegistry* mLayerManager = nullptr;

    std::unique_ptr<SoftParticlesDrawer>   mParticlesDrawer;
    std::unique_ptr<SoftConstraintsDrawer> mConstraintsDrawer;
    std::unique_ptr<SoftAnchorLinksDrawer> mAnchorLinksDrawer;
    std::unique_ptr<SoftVelocityDrawer>    mVelocityDrawer;
};

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
