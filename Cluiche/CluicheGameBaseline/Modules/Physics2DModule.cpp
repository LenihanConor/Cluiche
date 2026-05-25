#include "Modules/Physics2DModule.h"

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC Physics2DModule::kTypeId("Physics2DModule");

Physics2DModule::Physics2DModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
    mWorldDef.gravity = Dia::Maths::Vector2D(0.0f, -9.81f);
    mWorldDef.fixedTimestep = 1.0f / 30.0f;
    mWorldDef.maxSubSteps = 4;
    mWorldDef.broadPhase = nullptr;
}

Physics2DModule::~Physics2DModule() = default;

Dia::ApplicationFlow::StartResult Physics2DModule::DoStart()
{
    DIA_LOG_INFO("Application", "Physics2DModule::DoStart entry");
    mWorld = new Dia::RigidBody2D::PhysicsWorld(mWorldDef);
    DIA_LOG_INFO("Application", "Physics2DModule::DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void Physics2DModule::DoUpdate(float dt)
{
    mWorld->Update(dt);
}

Dia::ApplicationFlow::StopResult Physics2DModule::DoStop()
{
    DIA_LOG_INFO("Application", "Physics2DModule::DoStop entry");
    delete mWorld;
    mWorld = nullptr;
    DIA_LOG_INFO("Application", "Physics2DModule::DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using Physics2DModule_ = Cluiche::AppFlow::Physics2DModule; }
DIA_MODULE(Physics2DModule_);
