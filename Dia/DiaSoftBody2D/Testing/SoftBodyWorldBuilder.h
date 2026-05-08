#pragma once
#ifndef DIA_SOFTBODY2D_TESTING_SOFTBODYWORLDBUILDER_H
#define DIA_SOFTBODY2D_TESTING_SOFTBODYWORLDBUILDER_H

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaSoftBody2D/Rope.h>
#include <DiaSoftBody2D/Cloth.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace SoftBody2D { namespace Testing {

// ---------------------------------------------------------------------------
// SoftBodyWorldBuilder — fluent builder for SoftBodyWorld in tests
// ---------------------------------------------------------------------------

class SoftBodyWorldBuilder
{
public:
    SoftBodyWorldBuilder()
    {
        mDef.gravity          = Maths::Vector2D(0.0f, -9.81f);
        mDef.fixedTimestep    = 1.0f / 60.0f;
        mDef.maxSubSteps      = 8;
        mDef.solverIterations = 10;
        mDef.rigidBodyWorld   = nullptr;
    }

    SoftBodyWorldBuilder& WithGravity(const Maths::Vector2D& g)      { mDef.gravity          = g;       return *this; }
    SoftBodyWorldBuilder& WithTimestep(float dt)                      { mDef.fixedTimestep    = dt;      return *this; }
    SoftBodyWorldBuilder& WithMaxSubSteps(int n)                      { mDef.maxSubSteps      = n;       return *this; }
    SoftBodyWorldBuilder& WithSolverIterations(int n)                 { mDef.solverIterations = n;       return *this; }
    SoftBodyWorldBuilder& WithRigidBodyWorld(Dia::RigidBody2D::PhysicsWorld* w) { mDef.rigidBodyWorld = w; return *this; }
    SoftBodyWorldBuilder& WithNoGravity()                             { mDef.gravity = Maths::Vector2D(0.0f, 0.0f); return *this; }

    SoftBodyWorld* Build() const { return new SoftBodyWorld(mDef); }

private:
    WorldDef mDef;
};

// ---------------------------------------------------------------------------
// Pre-built WorldDef factories
// ---------------------------------------------------------------------------

inline WorldDef MakeStandardWorldDef(float dt = 1.0f / 60.0f)
{
    WorldDef def;
    def.gravity          = Maths::Vector2D(0.0f, -9.81f);
    def.fixedTimestep    = dt;
    def.maxSubSteps      = 8;
    def.solverIterations = 10;
    def.rigidBodyWorld   = nullptr;
    return def;
}

inline WorldDef MakeZeroGravWorldDef(float dt = 1.0f / 60.0f)
{
    WorldDef def = MakeStandardWorldDef(dt);
    def.gravity = Maths::Vector2D(0.0f, 0.0f);
    return def;
}

// ---------------------------------------------------------------------------
// Pre-built Def factories
// ---------------------------------------------------------------------------

inline RopeDef MakeStandardRope(int particles = 8, float length = 4.0f)
{
    RopeDef def;
    def.id             = Dia::Core::StringCRC("TestRope");
    def.startPoint     = Maths::Vector2D(0.0f, 0.0f);
    def.endPoint       = Maths::Vector2D(length, 0.0f);
    def.particleCount  = particles;
    def.mass           = 1.0f;
    def.stiffness      = 1.0f;
    def.particleRadius = 0.05f;
    def.maxStretch     = 0.0f;
    return def;
}

inline RopeDef MakeHeavyRope(int particles = 20, float mass = 5.0f)
{
    RopeDef def = MakeStandardRope(particles);
    def.mass = mass;
    return def;
}

inline ClothDef MakeStandardCloth(int resX = 5, int resY = 5, float width = 4.0f, float height = 4.0f)
{
    ClothDef def;
    def.id                  = Dia::Core::StringCRC("TestCloth");
    def.origin              = Maths::Vector2D(0.0f, 0.0f);
    def.width               = width;
    def.height              = height;
    def.resX                = resX;
    def.resY                = resY;
    def.mass                = 1.0f;
    def.structuralStiffness = 1.0f;
    def.shearStiffness      = 0.5f;
    def.bendStiffness       = 0.1f;
    def.particleRadius      = 0.05f;
    def.maxStretch          = 0.0f;
    def.pinTopRow           = false;
    return def;
}

inline ClothDef MakeStressCloth(int resX = 16, int resY = 16)
{
    return MakeStandardCloth(resX, resY, static_cast<float>(resX - 1), static_cast<float>(resY - 1));
}

}}} // namespace Dia::SoftBody2D::Testing

#endif // DIA_SOFTBODY2D_TESTING_SOFTBODYWORLDBUILDER_H
