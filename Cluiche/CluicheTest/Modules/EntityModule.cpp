#include "Modules/EntityModule.h"
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC EntityModule::kTypeId("EntityModule");

EntityModule::EntityModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{
}

EntityModule::~EntityModule() = default;

Dia::ApplicationFlow::StartResult EntityModule::DoStart()
{
    DIA_LOG_INFO("Application", "EntityModule::DoStart entry");

    mDomain.RegisterPool(new Dia::Entity::ComponentPool<Dia::Entity::Hierarchy::ParentComponent>(
        Dia::Entity::Hierarchy::ParentComponent::kTypeId));
    mDomain.RegisterPool(new Dia::Entity::ComponentPool<Dia::Entity::Hierarchy::ChildBufferComponent>(
        Dia::Entity::Hierarchy::ChildBufferComponent::kTypeId));

    mDomain.EndOfFrame();

    DIA_LOG_INFO("Application", "EntityModule::DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void EntityModule::DoUpdate(float dt)
{
    mDomain.Update(dt);
    mDomain.EndOfFrame();
}

Dia::ApplicationFlow::StopResult EntityModule::DoStop()
{
    DIA_LOG_INFO("Application", "EntityModule::DoStop entry");
    DIA_LOG_INFO("Application", "EntityModule::DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CluicheTest

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using EntityModule_ = CluicheTest::EntityModule; }
DIA_MODULE(EntityModule_);
