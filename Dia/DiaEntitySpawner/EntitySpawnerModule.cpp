#include <DiaEntitySpawner/EntitySpawnerModule.h>
#include <DiaEntity/Domain.h>

namespace Dia::EntitySpawner {

const Dia::Core::StringCRC EntitySpawnerModule::kInstanceId("entity-spawner-module");

EntitySpawnerModule::EntitySpawnerModule(Dia::Entity::Domain& domain)
    : Module(kInstanceId)
    , mDomain(domain)
{
}

Dia::ApplicationFlow::StartResult EntitySpawnerModule::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void EntitySpawnerModule::DoUpdate(float /*deltaTime*/)
{
}

Dia::ApplicationFlow::StopResult EntitySpawnerModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace Dia::EntitySpawner
