#include <gtest/gtest.h>

#include <DiaSoftBody2D/Rope.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

static RopeDef MakeDefaultRopeDef(int particleCount = 5, float mass = 1.0f)
{
    RopeDef def;
    def.id = Dia::Core::StringCRC("TestRope");
    def.startPoint = Vector2D(0.0f, 0.0f);
    def.endPoint = Vector2D(4.0f, 0.0f);
    def.particleCount = particleCount;
    def.mass = mass;
    def.stiffness = 1.0f;
    def.particleRadius = 0.1f;
    def.maxStretch = 0.0f;
    return def;
}

TEST(SoftBody2D_Rope, Construction_ParticleCountMatches)
{
    RopeDef def = MakeDefaultRopeDef(10);
    Rope rope(def);
    EXPECT_EQ(rope.GetParticleCount(), 10);
}

TEST(SoftBody2D_Rope, Construction_ConstraintCountIsParticleCountMinusOne)
{
    RopeDef def = MakeDefaultRopeDef(10);
    Rope rope(def);
    EXPECT_EQ(rope.GetConstraintCount(), 9);
}

TEST(SoftBody2D_Rope, Construction_ParticlesEvenlySpaced)
{
    RopeDef def = MakeDefaultRopeDef(5);
    Rope rope(def);

    EXPECT_FLOAT_EQ(rope.GetParticle(0).position.x, 0.0f);
    EXPECT_FLOAT_EQ(rope.GetParticle(4).position.x, 4.0f);
    EXPECT_NEAR(rope.GetParticle(2).position.x, 2.0f, 1e-5f);
}

TEST(SoftBody2D_Rope, Construction_InvMassDistributed)
{
    RopeDef def = MakeDefaultRopeDef(5, 5.0f);
    Rope rope(def);

    float expectedInvMass = 5.0f / 5.0f;
    EXPECT_FLOAT_EQ(rope.GetParticle(0).invMass, expectedInvMass);
    EXPECT_FLOAT_EQ(rope.GetParticle(2).invMass, expectedInvMass);
}

TEST(SoftBody2D_Rope, Construction_AnchorPinsSetInvMassZero)
{
    RopeDef def = MakeDefaultRopeDef(5);
    def.startAnchor = reinterpret_cast<Dia::RigidBody2D::Body2DBase*>(0x1);
    def.endAnchor = reinterpret_cast<Dia::RigidBody2D::Body2DBase*>(0x2);
    Rope rope(def);

    EXPECT_FLOAT_EQ(rope.GetParticle(0).invMass, 0.0f);
    EXPECT_FLOAT_EQ(rope.GetParticle(4).invMass, 0.0f);
    EXPECT_GT(rope.GetParticle(2).invMass, 0.0f);
}

TEST(SoftBody2D_Rope, GetBodyType_ReturnsRope)
{
    RopeDef def = MakeDefaultRopeDef();
    Rope rope(def);
    EXPECT_EQ(rope.GetBodyType(), BodyType::kRope);
}

TEST(SoftBody2D_Rope, GetId_MatchesDef)
{
    RopeDef def = MakeDefaultRopeDef();
    Rope rope(def);
    EXPECT_EQ(rope.GetId(), Dia::Core::StringCRC("TestRope"));
}

TEST(SoftBody2D_Rope, IsTorn_InitiallyFalse)
{
    RopeDef def = MakeDefaultRopeDef();
    Rope rope(def);
    EXPECT_FALSE(rope.IsTorn());
}

TEST(SoftBody2D_Rope, CheckTearing_NoMaxStretch_NeverTears)
{
    RopeDef def = MakeDefaultRopeDef(3);
    def.maxStretch = 0.0f;
    Rope rope(def);

    rope.GetParticle(2).position = Vector2D(100.0f, 0.0f);
    rope.CheckTearing();
    EXPECT_FALSE(rope.IsTorn());
}

TEST(SoftBody2D_Rope, CheckTearing_ExceedsMaxStretch_TearsPermanently)
{
    RopeDef def = MakeDefaultRopeDef(3);
    def.maxStretch = 0.01f;
    Rope rope(def);

    rope.GetParticle(2).position = Vector2D(100.0f, 0.0f);
    rope.CheckTearing();
    EXPECT_TRUE(rope.IsTorn());
}

TEST(SoftBody2D_Rope, CheckTearing_ConstraintDeactivated)
{
    RopeDef def = MakeDefaultRopeDef(3);
    def.maxStretch = 0.01f;
    Rope rope(def);

    rope.GetParticle(2).position = Vector2D(100.0f, 0.0f);
    rope.CheckTearing();

    bool anyInactive = false;
    for (int i = 0; i < rope.GetConstraintCount(); ++i)
    {
        if (!rope.GetConstraint(i).active)
            anyInactive = true;
    }
    EXPECT_TRUE(anyInactive);
}

TEST(SoftBody2D_Rope, ConstraintRestLength_MatchesSegmentLength)
{
    RopeDef def = MakeDefaultRopeDef(5);
    Rope rope(def);

    float expectedSegLen = 4.0f / 4.0f;
    for (int i = 0; i < rope.GetConstraintCount(); ++i)
    {
        EXPECT_NEAR(rope.GetConstraint(i).restLength, expectedSegLen, 1e-5f);
    }
}

TEST(SoftBody2D_Rope, Anchors_ReturnSetValues)
{
    auto* fakeAnchor = reinterpret_cast<Dia::RigidBody2D::Body2DBase*>(0xBEEF);
    RopeDef def = MakeDefaultRopeDef();
    def.startAnchor = fakeAnchor;
    def.endAnchor = nullptr;
    Rope rope(def);

    EXPECT_EQ(rope.GetStartAnchor(), fakeAnchor);
    EXPECT_EQ(rope.GetEndAnchor(), nullptr);
}

TEST(SoftBody2D_Rope, GetStiffness_MatchesDef)
{
    RopeDef def = MakeDefaultRopeDef();
    def.stiffness = 0.75f;
    Rope rope(def);
    EXPECT_FLOAT_EQ(rope.GetStiffness(), 0.75f);
}

TEST(SoftBody2D_Rope, GetMaxStretch_MatchesDef)
{
    RopeDef def = MakeDefaultRopeDef();
    def.maxStretch = 0.5f;
    Rope rope(def);
    EXPECT_FLOAT_EQ(rope.GetMaxStretch(), 0.5f);
}

TEST(SoftBody2D_Rope, UniqueId_HasValue)
{
    EXPECT_EQ(Rope::kUniqueId, Dia::Core::StringCRC("Rope"));
}

TEST(SoftBody2D_Rope, TwoParticleMinimum_ValidConstruction)
{
    RopeDef def = MakeDefaultRopeDef(2);
    Rope rope(def);
    EXPECT_EQ(rope.GetParticleCount(), 2);
    EXPECT_EQ(rope.GetConstraintCount(), 1);
}

TEST(SoftBody2D_Rope, ConstraintType_IsRope)
{
    RopeDef def = MakeDefaultRopeDef(3);
    Rope rope(def);
    for (int i = 0; i < rope.GetConstraintCount(); ++i)
    {
        EXPECT_EQ(rope.GetConstraint(i).type, ConstraintType::kRope);
    }
}

TEST(SoftBody2D_Rope, ConstraintStiffness_MatchesDef)
{
    RopeDef def = MakeDefaultRopeDef(3);
    def.stiffness = 0.8f;
    Rope rope(def);
    for (int i = 0; i < rope.GetConstraintCount(); ++i)
    {
        EXPECT_FLOAT_EQ(rope.GetConstraint(i).stiffness, 0.8f);
    }
}
