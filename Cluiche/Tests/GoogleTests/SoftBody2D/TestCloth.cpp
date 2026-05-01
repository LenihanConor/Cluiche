#include <gtest/gtest.h>

#include <DiaSoftBody2D/Cloth.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::SoftBody2D;
using namespace Dia::Maths;

static ClothDef MakeDefaultClothDef(int resX = 4, int resY = 4)
{
    ClothDef def;
    def.id = Dia::Core::StringCRC("TestCloth");
    def.origin = Vector2D(0.0f, 0.0f);
    def.width = 3.0f;
    def.height = 3.0f;
    def.resX = resX;
    def.resY = resY;
    def.mass = 1.0f;
    def.structuralStiffness = 1.0f;
    def.shearStiffness = 0.5f;
    def.bendStiffness = 0.1f;
    def.particleRadius = 0.05f;
    def.maxStretch = 0.0f;
    def.pinTopRow = false;
    return def;
}

TEST(SoftBody2D_Cloth, Construction_ParticleCountMatchesGrid)
{
    ClothDef def = MakeDefaultClothDef(4, 4);
    Cloth cloth(def);
    EXPECT_EQ(cloth.GetParticleCount(), 16);
    EXPECT_EQ(cloth.GetResX(), 4);
    EXPECT_EQ(cloth.GetResY(), 4);
}

TEST(SoftBody2D_Cloth, Construction_ConstraintsCreated)
{
    ClothDef def = MakeDefaultClothDef(4, 4);
    Cloth cloth(def);
    EXPECT_GT(cloth.GetConstraintCount(), 0);
}

TEST(SoftBody2D_Cloth, GetBodyType_ReturnsCloth)
{
    ClothDef def = MakeDefaultClothDef();
    Cloth cloth(def);
    EXPECT_EQ(cloth.GetBodyType(), BodyType::kCloth);
}

TEST(SoftBody2D_Cloth, GetId_MatchesDef)
{
    ClothDef def = MakeDefaultClothDef();
    Cloth cloth(def);
    EXPECT_EQ(cloth.GetId(), Dia::Core::StringCRC("TestCloth"));
}

TEST(SoftBody2D_Cloth, PinTopRow_SetsInvMassZeroForTopRow)
{
    ClothDef def = MakeDefaultClothDef(4, 4);
    def.pinTopRow = true;
    Cloth cloth(def);

    for (int x = 0; x < 4; ++x)
    {
        EXPECT_FLOAT_EQ(cloth.GetParticle(x, 0).invMass, 0.0f);
    }
    EXPECT_GT(cloth.GetParticle(0, 1).invMass, 0.0f);
}

TEST(SoftBody2D_Cloth, PinParticle_SetsInvMassZero)
{
    ClothDef def = MakeDefaultClothDef(4, 4);
    Cloth cloth(def);

    EXPECT_GT(cloth.GetParticle(2, 2).invMass, 0.0f);
    cloth.PinParticle(2, 2);
    EXPECT_FLOAT_EQ(cloth.GetParticle(2, 2).invMass, 0.0f);
}

TEST(SoftBody2D_Cloth, UnpinParticle_RestoresOriginalInvMass)
{
    ClothDef def = MakeDefaultClothDef(4, 4);
    Cloth cloth(def);

    float originalInvMass = cloth.GetParticle(2, 2).invMass;
    cloth.PinParticle(2, 2);
    EXPECT_FLOAT_EQ(cloth.GetParticle(2, 2).invMass, 0.0f);

    cloth.UnpinParticle(2, 2);
    EXPECT_FLOAT_EQ(cloth.GetParticle(2, 2).invMass, originalInvMass);
}

TEST(SoftBody2D_Cloth, CheckTearing_NoMaxStretch_NeverTears)
{
    ClothDef def = MakeDefaultClothDef(3, 3);
    def.maxStretch = 0.0f;
    Cloth cloth(def);

    cloth.GetParticle(2, 2).position = Vector2D(100.0f, 100.0f);
    cloth.CheckTearing();
    EXPECT_FALSE(cloth.IsTorn());
}

TEST(SoftBody2D_Cloth, CheckTearing_ExceedsMaxStretch_Tears)
{
    ClothDef def = MakeDefaultClothDef(3, 3);
    def.maxStretch = 0.01f;
    Cloth cloth(def);

    cloth.GetParticle(2, 2).position = Vector2D(100.0f, 100.0f);
    cloth.CheckTearing();
    EXPECT_TRUE(cloth.IsTorn());
}

TEST(SoftBody2D_Cloth, Construction_CornerPositions)
{
    ClothDef def = MakeDefaultClothDef(4, 4);
    Cloth cloth(def);

    EXPECT_NEAR(cloth.GetParticle(0, 0).position.x, 0.0f, 1e-5f);
    EXPECT_NEAR(cloth.GetParticle(0, 0).position.y, 0.0f, 1e-5f);
    EXPECT_NEAR(cloth.GetParticle(3, 0).position.x, 3.0f, 1e-5f);
    EXPECT_NEAR(cloth.GetParticle(0, 3).position.y, -3.0f, 1e-5f);
}

TEST(SoftBody2D_Cloth, UniqueId_HasValue)
{
    EXPECT_EQ(Cloth::kUniqueId, Dia::Core::StringCRC("Cloth"));
}

TEST(SoftBody2D_Cloth, AllThreeConstraintTypesPresent_3x3Grid)
{
    ClothDef def = MakeDefaultClothDef(3, 3);
    Cloth cloth(def);

    bool hasStructural = false, hasShear = false, hasBend = false;
    for (int i = 0; i < cloth.GetConstraintCount(); ++i)
    {
        ConstraintType t = cloth.GetConstraint(i).type;
        if (t == ConstraintType::kStructural) hasStructural = true;
        if (t == ConstraintType::kShear)      hasShear = true;
        if (t == ConstraintType::kBend)        hasBend = true;
    }
    EXPECT_TRUE(hasStructural);
    EXPECT_TRUE(hasShear);
    EXPECT_TRUE(hasBend);
}

TEST(SoftBody2D_Cloth, ConstraintCount_3x3Grid_PredictableCount)
{
    ClothDef def = MakeDefaultClothDef(3, 3);
    Cloth cloth(def);

    // 3x3 grid:
    // Structural H: 3 rows * 2 = 6
    // Structural V: 2 rows * 3 = 6
    // Shear diag-right: 2*2 = 4
    // Shear diag-left: 2*2 = 4
    // Bend H: 3 rows * 1 = 3
    // Bend V: 1 row * 3 = 3
    // Total = 26
    EXPECT_EQ(cloth.GetConstraintCount(), 26);
}

TEST(SoftBody2D_Cloth, DoublePinUnpin_Idempotent)
{
    ClothDef def = MakeDefaultClothDef(4, 4);
    Cloth cloth(def);

    float originalInvMass = cloth.GetParticle(1, 1).invMass;
    cloth.PinParticle(1, 1);
    cloth.PinParticle(1, 1);
    EXPECT_FLOAT_EQ(cloth.GetParticle(1, 1).invMass, 0.0f);

    cloth.UnpinParticle(1, 1);
    EXPECT_FLOAT_EQ(cloth.GetParticle(1, 1).invMass, originalInvMass);
}
