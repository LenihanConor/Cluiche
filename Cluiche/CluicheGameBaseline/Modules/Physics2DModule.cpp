#include "Modules/Physics2DModule.h"

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaObservation/Log/DiaLog.h>

#ifdef DIA_DEBUG
#include <DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h>
#include <DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.h>
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC Physics2DModule::kTypeId("Physics2DModule");

Physics2DModule::Physics2DModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
    mWorldDef.gravity = Dia::Maths::Vector2D(0.0f, -9.81f);
    mWorldDef.fixedTimestep = 1.0f / 30.0f;
    mWorldDef.maxSubSteps = 4;
    mWorldDef.broadPhase = nullptr;   // assigned in DoStart once the grid exists
}

Physics2DModule::~Physics2DModule() = default;

Dia::ApplicationFlow::StartResult Physics2DModule::DoStart()
{
    DIA_LOG_INFO("Application", "Physics2DModule::DoStart entry");

    // Build the broadphase grid before the world so bodies register in it on
    // AddRigidBody. Bounds cover the test/game working area; cellSize is chosen
    // to keep cell count well under SpatialGrid's internal cap (40x40 = 1600).
    BroadPhaseGrid::Def gridDef;
    gridDef.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(-2000.0f, -2000.0f),
        Dia::Maths::Vector2D( 2000.0f,  2000.0f));
    gridDef.cellSize = 100.0f;
    mBroadPhase = std::make_unique<BroadPhaseGrid>(gridDef);
    mWorldDef.broadPhase = mBroadPhase.get();

    mWorld = new Dia::RigidBody2D::PhysicsWorld(mWorldDef);
    DIA_LOG_INFO("Application", "Physics2DModule::DoStart exit (broadphase active)");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void Physics2DModule::DoUpdate(float dt)
{
#ifdef DIA_DEBUG
    if (!mShapesDrawer)
        RegisterDrawers();
#endif

    mWorld->Update(dt);
}

Dia::ApplicationFlow::StopResult Physics2DModule::DoStop()
{
    DIA_LOG_INFO("Application", "Physics2DModule::DoStop entry");

#ifdef DIA_DEBUG
    if (mShapesDrawer)
    {
        // Only unregister if VisualDebuggerModule is still active (concurrent stop).
        if (auto* vd = mVisualDebuggerRef.Get())
        {
            auto& mgr = vd->GetLayerManager();
            mgr.Unregister(mShapesDrawer->GetLayerName());
            mgr.Unregister(mVelocityDrawer->GetLayerName());
            mgr.Unregister(mContactsDrawer->GetLayerName());
            mgr.Unregister(mAABBDrawer->GetLayerName());
            mgr.Unregister(mConstraintsDrawer->GetLayerName());
        }
        mShapesDrawer.reset();
        mVelocityDrawer.reset();
        mContactsDrawer.reset();
        mAABBDrawer.reset();
        mConstraintsDrawer.reset();
    }
#endif

    // World holds handles into the grid — destroy it first, then the grid.
    delete mWorld;
    mWorld = nullptr;
    mWorldDef.broadPhase = nullptr;
    mBroadPhase.reset();
    DIA_LOG_INFO("Application", "Physics2DModule::DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

#ifdef DIA_DEBUG
void Physics2DModule::RegisterDrawers()
{
    auto* vd = mVisualDebuggerRef.Get();
    if (!vd)
        return;

    auto& mgr = vd->GetLayerManager();
    const Dia::Core::StringCRC stageTag("RigidBody2D");

    mShapesDrawer      = std::make_unique<Dia::RigidBody2D::PhysicsShapesDrawer>(*mWorld, mgr);
    mVelocityDrawer    = std::make_unique<Dia::RigidBody2D::VelocityArrowsDrawer>(*mWorld, mgr);
    mContactsDrawer    = std::make_unique<Dia::RigidBody2D::ContactNormalsDrawer>(*mWorld, mgr);
    mAABBDrawer        = std::make_unique<Dia::RigidBody2D::PhysicsAABBDrawer>(*mWorld, mgr);
    mConstraintsDrawer = std::make_unique<Dia::RigidBody2D::ConstraintLinesDrawer>(*mWorld, mgr);

    mgr.Register(mShapesDrawer.get(),      10, stageTag);
    mgr.Register(mVelocityDrawer.get(),    11, stageTag);
    mgr.Register(mContactsDrawer.get(),    12, stageTag);
    mgr.Register(mAABBDrawer.get(),        13, stageTag);
    mgr.Register(mConstraintsDrawer.get(), 14, stageTag);
}
#endif

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using Physics2DModule_ = Cluiche::AppFlow::Physics2DModule; }
DIA_MODULE(Physics2DModule_);
DIA_DESCRIBE(Physics2DModule_::kTypeId, "Runs the 2D rigid-body physics simulation and publishes physics results via FrameStream.");
