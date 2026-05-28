#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaRigidBody2D/World/WorldDef.h>

#ifdef DIA_DEBUG
#include <memory>
#endif

namespace Dia::RigidBody2D { class PhysicsWorld; }

#ifdef DIA_DEBUG
namespace Dia::RigidBody2D
{
    class PhysicsShapesDrawer;
    class VelocityArrowsDrawer;
    class ContactNormalsDrawer;
    class PhysicsAABBDrawer;
    class ConstraintLinesDrawer;
}
#endif

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

#ifdef DIA_DEBUG
    void RegisterDrawers();

    std::unique_ptr<Dia::RigidBody2D::PhysicsShapesDrawer>   mShapesDrawer;
    std::unique_ptr<Dia::RigidBody2D::VelocityArrowsDrawer>  mVelocityDrawer;
    std::unique_ptr<Dia::RigidBody2D::ContactNormalsDrawer>  mContactsDrawer;
    std::unique_ptr<Dia::RigidBody2D::PhysicsAABBDrawer>     mAABBDrawer;
    std::unique_ptr<Dia::RigidBody2D::ConstraintLinesDrawer> mConstraintsDrawer;
#endif
};

} } // namespace Cluiche::AppFlow
