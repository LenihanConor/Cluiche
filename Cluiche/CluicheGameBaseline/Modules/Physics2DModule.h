#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaRigidBody2D/World/WorldDef.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <memory>

#ifdef DIA_DEBUG
#include "Modules/VisualDebuggerModule.h"
#include <DiaRigidBody2DVisualDebugger/RigidBody2DDebugDomain.h>
#endif

namespace Dia::RigidBody2D { class PhysicsWorld; class Body2DBase; }

namespace Cluiche { namespace AppFlow {

class Physics2DModule : public Dia::ApplicationFlow::Module {
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit Physics2DModule(const Dia::Core::StringCRC& instanceId);
    ~Physics2DModule() override;

    Dia::RigidBody2D::PhysicsWorld* GetWorld() const { return mWorld; }

protected:
    Dia::ApplicationFlow::StartResult DoStart()          override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop()           override;

private:
    // Broadphase acceleration structure for collision/query/trigger detection.
    // Owned here and assigned (non-owning) into mWorldDef.broadPhase, so it must
    // outlive mWorld. Without it the world falls back to an O(n^2) pair sweep.
    using BroadPhaseGrid = Dia::Geometry2D::SpatialGrid<Dia::RigidBody2D::Body2DBase*>;
    std::unique_ptr<BroadPhaseGrid> mBroadPhase;

    Dia::RigidBody2D::PhysicsWorld* mWorld = nullptr;
    Dia::RigidBody2D::WorldDef mWorldDef;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<VisualDebuggerModule> mVisualDebuggerRef{this};

    // Registers the physics debug domain once VisualDebuggerModule is available.
    void RegisterDebugDomain();

    std::unique_ptr<Dia::RigidBody2D::RigidBody2DDebugDomain> mDebugDomain;
#endif
};

} } // namespace Cluiche::AppFlow
