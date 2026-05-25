#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaRigidBody2D/World/WorldDef.h>

namespace Dia::RigidBody2D { class PhysicsWorld; }

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
    Dia::RigidBody2D::PhysicsWorld* mWorld = nullptr;
    Dia::RigidBody2D::WorldDef mWorldDef;
};

} } // namespace Cluiche::AppFlow
