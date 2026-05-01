#include <gtest/gtest.h>

#include <DiaSoftBody2D/Particle.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

TEST(SoftBody2D_Particle, DefaultConstruction_ZeroFields)
{
    Particle p{};
    EXPECT_FLOAT_EQ(p.position.x, 0.0f);
    EXPECT_FLOAT_EQ(p.position.y, 0.0f);
    EXPECT_FLOAT_EQ(p.prevPosition.x, 0.0f);
    EXPECT_FLOAT_EQ(p.prevPosition.y, 0.0f);
    EXPECT_FLOAT_EQ(p.invMass, 0.0f);
    EXPECT_FLOAT_EQ(p.radius, 0.0f);
}

TEST(SoftBody2D_Particle, DeriveVelocity_StationaryParticle_ZeroVelocity)
{
    Particle p{};
    p.position = Vector2D(5.0f, 3.0f);
    p.prevPosition = Vector2D(5.0f, 3.0f);

    Vector2D vel = DeriveVelocity(p, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(vel.x, 0.0f);
    EXPECT_FLOAT_EQ(vel.y, 0.0f);
}

TEST(SoftBody2D_Particle, DeriveVelocity_MovingParticle_CorrectVelocity)
{
    Particle p{};
    float dt = 1.0f / 60.0f;
    p.position = Vector2D(1.0f, 0.0f);
    p.prevPosition = Vector2D(0.0f, 0.0f);

    Vector2D vel = DeriveVelocity(p, dt);
    EXPECT_FLOAT_EQ(vel.x, 60.0f);
    EXPECT_FLOAT_EQ(vel.y, 0.0f);
}

TEST(SoftBody2D_Particle, DeriveVelocity_NegativeDirection)
{
    Particle p{};
    float dt = 0.5f;
    p.position = Vector2D(0.0f, -2.0f);
    p.prevPosition = Vector2D(0.0f, 0.0f);

    Vector2D vel = DeriveVelocity(p, dt);
    EXPECT_FLOAT_EQ(vel.x, 0.0f);
    EXPECT_FLOAT_EQ(vel.y, -4.0f);
}

TEST(SoftBody2D_Particle, DeriveVelocity_VerySmallTimestep_NumericalStability)
{
    Particle p{};
    float dt = 1e-6f;
    p.position = Vector2D(1.0f, 0.0f);
    p.prevPosition = Vector2D(0.0f, 0.0f);

    Vector2D vel = DeriveVelocity(p, dt);
    EXPECT_GT(vel.x, 0.0f);
    EXPECT_FALSE(std::isinf(vel.x));
    EXPECT_FALSE(std::isnan(vel.x));
}
