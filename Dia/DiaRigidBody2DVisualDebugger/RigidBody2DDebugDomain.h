////////////////////////////////////////////////////////////////////////////////
// Filename: RigidBody2DDebugDomain.h
// Description: IDebugDomain implementation for the 2D rigid-body physics
//              subsystem. Owns and bulk-registers the five world-space
//              physics drawers and bridges their state to DiaDebugPanel.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>

#include <memory>

namespace Dia::Debug { class IDebugLayerRegistry; }

namespace Dia::RigidBody2D
{
    class PhysicsWorld;
    class PhysicsShapesDrawer;
    class VelocityArrowsDrawer;
    class ContactNormalsDrawer;
    class PhysicsAABBDrawer;
    class ConstraintLinesDrawer;
}

namespace Dia::RigidBody2D
{

////////////////////////////////////////////////////////////////////////////////
// RigidBody2DDebugDomain
//
// Reference IDebugDomain migration. The drawers are owned here and created
// lazily inside Register() — they need the DebugLayerManager as their
// IDebugContext, which is only supplied at registration time.
//
// Panel-facing drawer names are short labels ("Shapes", "Velocity", ...).
// The authoritative layer key handed to the DebugLayerManager is always
// IVisualDebugger::GetLayerName(); OnCommand() resolves a panel label back to
// that key through the same table used by GetJSONState().
////////////////////////////////////////////////////////////////////////////////
class RigidBody2DDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 5;

    explicit RigidBody2DDebugDomain(const PhysicsWorld& world);
    ~RigidBody2DDebugDomain() override;

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
    // Maps a panel drawer label to the layer key registered with the manager.
    // Returns StringCRC::kZero-equivalent (default StringCRC) when unmatched.
    Dia::Core::StringCRC ResolveLayerName(const char* drawerName) const;

    const PhysicsWorld&            mWorld;
    Dia::Debug::IDebugLayerRegistry* mLayerManager = nullptr;

    float mParamVelocityScale = 1.0f;
    float mParamNormalLength  = 1.0f;

    std::unique_ptr<PhysicsShapesDrawer>   mShapesDrawer;
    std::unique_ptr<VelocityArrowsDrawer>  mVelocityDrawer;
    std::unique_ptr<ContactNormalsDrawer>  mContactsDrawer;
    std::unique_ptr<PhysicsAABBDrawer>     mAABBDrawer;
    std::unique_ptr<ConstraintLinesDrawer> mConstraintsDrawer;
};

} // namespace Dia::RigidBody2D

#endif // DIA_DEBUG
