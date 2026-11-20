#include "Modules/EntityModule.h"
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/Hierarchy/ParentComponent.h>
#include <DiaEntity/Hierarchy/ChildBufferComponent.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC EntityModule::kTypeId("EntityModule");

EntityModule::EntityModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
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

    mReady = true;
    DIA_LOG_INFO("Application", "EntityModule::DoStart exit");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void EntityModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    const float dt = ctx.gameDt.AsFloatInSeconds();
    mDomain.Update(dt);
    mDomain.EndOfFrame();
}

Dia::ApplicationFlow::StopResult EntityModule::DoStop()
{
    DIA_LOG_INFO("Application", "EntityModule::DoStop entry");
    mReady = false;

    // The module instance (and its Domain) outlives the stage, so entities a
    // stage created would leak into the next stage's world. Destroy them all.
    unsigned int destroyed = 0;
    for (uint32_t i = 0; i < Dia::Entity::kMaxEntitiesPerDomain; ++i)
    {
        const Dia::Entity::Entity e = mDomain.GetAliveEntity(i);
        if (e.IsValid())
        {
            mDomain.QueueDestroy(e);
            ++destroyed;
        }
    }
    mDomain.EndOfFrame();
    DIA_LOG_INFO("Application", "EntityModule::DoStop destroyed %u entities", destroyed);
    DIA_LOG_INFO("Application", "EntityModule::DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using EntityModule_ = Cluiche::AppFlow::EntityModule; }
DIA_MODULE(EntityModule_);
DIA_DESCRIBE(EntityModule_::kTypeId, "Hosts the entity world: creates, updates, and destroys diaentitytemplate instances and their components.");
