#pragma once
#ifndef DIA_RIGIDBODY2D_TESTING_PHYSICSWORLDBUILDER_H
#define DIA_RIGIDBODY2D_TESTING_PHYSICSWORLDBUILDER_H

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Spatial/ISpatialStructure.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace RigidBody2D { namespace Testing {

// ---------------------------------------------------------------------------
// PhysicsWorldBuilder — fluent builder for PhysicsWorld in tests
// ---------------------------------------------------------------------------

class PhysicsWorldBuilder
{
public:
    PhysicsWorldBuilder()
    {
        mDef.gravity       = Maths::Vector2D(0.0f, -9.81f);
        mDef.fixedTimestep = 1.0f / 60.0f;
        mDef.maxSubSteps   = 8;
        mDef.broadPhase    = nullptr;
    }

    PhysicsWorldBuilder& WithGravity(const Maths::Vector2D& g)   { mDef.gravity       = g;  return *this; }
    PhysicsWorldBuilder& WithTimestep(float dt)                   { mDef.fixedTimestep = dt; return *this; }
    PhysicsWorldBuilder& WithMaxSubSteps(int n)                   { mDef.maxSubSteps   = n;  return *this; }
    PhysicsWorldBuilder& WithBroadPhase(Geometry2D::ISpatialStructure<Body2DBase*>* bp) { mDef.broadPhase = bp; return *this; }
    PhysicsWorldBuilder& WithNoGravity()                          { mDef.gravity = Maths::Vector2D(0.0f, 0.0f); return *this; }

    PhysicsWorld* Build() const { return new PhysicsWorld(mDef); }

private:
    WorldDef mDef;
};

// ---------------------------------------------------------------------------
// Pre-built WorldDef factories (for use without the builder)
// ---------------------------------------------------------------------------

inline WorldDef MakeStandardWorldDef(float dt = 1.0f / 60.0f)
{
    WorldDef def;
    def.gravity       = Maths::Vector2D(0.0f, -9.81f);
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    return def;
}

inline WorldDef MakeZeroGravWorldDef(float dt = 1.0f / 60.0f)
{
    WorldDef def = MakeStandardWorldDef(dt);
    def.gravity = Maths::Vector2D(0.0f, 0.0f);
    return def;
}

// ---------------------------------------------------------------------------
// BodyFactory — creates test bodies with minimal boilerplate
// ---------------------------------------------------------------------------

class BodyFactory
{
public:
    static constexpr int kMaxSlots = 64;

    struct Slot
    {
        Geometry2D::Transform transform;
        Geometry2D::Circle    circle;
        Geometry2D::AARect    broad;
    };

    explicit BodyFactory(PhysicsWorld& world) : mWorld(world), mNext(0) {}

    PointBody2D* MakePoint(float x, float y, float mass = 1.0f, bool allowSleep = false)
    {
        PointBodyDef def;
        def.id            = Dia::Core::StringCRC(mNext + 1000u);
        def.type          = BodyType::kDynamic;
        def.mass          = mass;
        def.allowSleeping = allowSleep;
        PointBody2D* body = mWorld.AddPointBody(def);
        if (body)
            body->GetTransform()->SetLocalPosition(Maths::Vector2D(x, y));
        ++mNext;
        return body;
    }

    RigidBody2D* MakeDynamic(float x, float y, float radius = 0.5f, float mass = 1.0f)
    {
        return MakeBody(x, y, radius, mass, BodyType::kDynamic);
    }

    RigidBody2D* MakeStatic(float x, float y, float radius = 0.5f)
    {
        return MakeBody(x, y, radius, 0.0f, BodyType::kStatic);
    }

private:
    RigidBody2D* MakeBody(float x, float y, float radius, float mass, BodyType type)
    {
        if (mNext >= kMaxSlots) return nullptr;
        Slot& s       = mSlots[mNext];
        s.circle      = Geometry2D::Circle(radius, Maths::Vector2D(x, y));
        s.broad       = Geometry2D::AARect(Maths::Vector2D(x - radius, y - radius),
                                           Maths::Vector2D(x + radius, y + radius));
        s.transform.SetLocalPosition(Maths::Vector2D(x, y));

        RigidBodyDef def;
        def.id            = Dia::Core::StringCRC(mNext + 2000u);
        def.type          = type;
        def.mass          = mass;
        def.allowSleeping = false;
        def.circleShape   = &s.circle;
        def.transform     = &s.transform;

        RigidBody2D* body = mWorld.AddRigidBody(def);
        ++mNext;
        return body;
    }

    PhysicsWorld& mWorld;
    Slot          mSlots[kMaxSlots];
    int           mNext;
};

}}} // namespace Dia::RigidBody2D::Testing

#endif // DIA_RIGIDBODY2D_TESTING_PHYSICSWORLDBUILDER_H
