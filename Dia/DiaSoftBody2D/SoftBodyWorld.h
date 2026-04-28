#pragma once

#include "DiaSoftBody2D/Particle.h"
#include "DiaSoftBody2D/SoftBody.h"
#include "DiaSoftBody2D/Rope.h"
#include "DiaSoftBody2D/Cloth.h"
#include "DiaCore/Containers/Arrays/DynamicArray.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::Geometry2D { class AARect; class Circle; class Line; }
namespace Dia::RigidBody2D { class PhysicsWorld; }

namespace Dia::SoftBody2D {

struct WorldDef {
    Dia::Maths::Vector2D              gravity          = { 0.0f, -9.81f };
    float                             fixedTimestep    = 1.0f / 60.0f;
    int                               maxSubSteps      = 8;
    int                               solverIterations = 10;
    // Non-owning; optional.
    // When non-null, enables particle vs rigid body collision and anchor coupling.
    // Call RigidBodyWorld::Update() BEFORE SoftBodyWorld::Update() each frame.
    Dia::RigidBody2D::PhysicsWorld*   rigidBodyWorld   = nullptr;
};

class SoftBodyWorld {
public:
    explicit SoftBodyWorld(const WorldDef& def);
    ~SoftBodyWorld();

    Rope*  AddRope(const RopeDef& def);
    Cloth* AddCloth(const ClothDef& def);
    void   RemoveBody(SoftBody* body);
    int    GetBodyCount() const;

    void AddStaticShape(const Dia::Geometry2D::AARect* shape);
    void AddStaticShape(const Dia::Geometry2D::Circle* shape);
    void AddStaticShape(const Dia::Geometry2D::Line*   shape);
    void RemoveStaticShape(const void* shapePtr);

    void Update(float deltaTime);

    void                        SetGravity(const Dia::Maths::Vector2D& gravity);
    const Dia::Maths::Vector2D& GetGravity() const;

    const Dia::Core::Containers::DynamicArray<SoftBody*>& GetBodies() const;

private:
    void StepOnce();

    void ApplyExternalForces();
    void ProjectConstraints();
    void ResolveGeometryCollision();
    void ResolveRigidBodyCollision();
    void FinalizeVelocities();
    void CheckTearing();

    WorldDef                                                        mDef;
    float                                                           mAccumulator;
    Dia::Core::Containers::DynamicArray<SoftBody*>                  mBodies;
    Dia::Core::Containers::DynamicArray<const Dia::Geometry2D::AARect*>  mStaticRects;
    Dia::Core::Containers::DynamicArray<const Dia::Geometry2D::Circle*>  mStaticCircles;
    Dia::Core::Containers::DynamicArray<const Dia::Geometry2D::Line*>    mStaticLines;
};

} // namespace Dia::SoftBody2D
