#include <gtest/gtest.h>
#include <DiaMaths/Core/HalfFloat.h>

using namespace Dia::Maths;

TEST(HalfFloat, DefaultConstruction_IsZero)
{
    HalfFloat a;

    EXPECT_EQ(a, 0.0f);
}

TEST(HalfFloat, ConstructWithFloat_StoresValue)
{
    HalfFloat b(2.0f);

    EXPECT_EQ(b, 2.0f);
    EXPECT_EQ(b, HalfFloat(2.0f));
}

TEST(HalfFloat, UnaryNegation_FlipsSign)
{
    HalfFloat a(1.0f);
    HalfFloat b = -a;

    EXPECT_EQ(a, 1.0f);
    EXPECT_EQ(b, -1.0f);
}

TEST(HalfFloat, Assignment_CopiesValue)
{
    HalfFloat c = HalfFloat(2.0f);

    EXPECT_EQ(c, 2.0f);
}

TEST(HalfFloat, AdditionAndSubtraction_WorksCorrectly)
{
    HalfFloat a(1.0f);
    HalfFloat b(2.0f);
    HalfFloat c = a + b;
    HalfFloat d = a - b;

    EXPECT_EQ(a, 1.0f);
    EXPECT_EQ(b, 2.0f);
    EXPECT_EQ(c, 3.0f);
    EXPECT_EQ(d, -1.0f);
}

TEST(HalfFloat, CompoundAddition_WorksCorrectly)
{
    HalfFloat a(1.0f);
    HalfFloat e;
    HalfFloat f;

    e += HalfFloat(2.0f);
    f += a;

    EXPECT_EQ(e, 2.0f);
    EXPECT_EQ(f, 1.0f);
}

TEST(HalfFloat, CompoundSubtraction_WorksCorrectly)
{
    HalfFloat a(1.0f);
    HalfFloat g;
    HalfFloat h;

    g -= HalfFloat(2.0f);
    h -= a;

    EXPECT_EQ(g, -2.0f);
    EXPECT_EQ(h, -1.0f);
}

TEST(HalfFloat, SelfNegation_FlipsSign)
{
    HalfFloat i(1.0f);

    i = -i;

    EXPECT_EQ(i, -1.0f);
}

TEST(HalfFloat, MultiplicationAndDivision_WorksCorrectly)
{
    HalfFloat a(4.0f);
    HalfFloat b(2.0f);
    HalfFloat c = a * b;
    HalfFloat d = a / b;

    EXPECT_EQ(a, 4.0f);
    EXPECT_EQ(b, 2.0f);
    EXPECT_EQ(c, 8.0f);
    EXPECT_EQ(d, 2.0f);
}

TEST(HalfFloat, CompoundDivision_WorksCorrectly)
{
    HalfFloat e(6.0f);

    e /= HalfFloat(2.0f);

    EXPECT_EQ(e, 3.0f);
}

TEST(HalfFloat, CompoundMultiplication_WorksCorrectly)
{
    HalfFloat g(3.0f);

    g *= HalfFloat(2.0f);

    EXPECT_EQ(g, 6.0f);
}

TEST(HalfFloat, Round_RoundsToDecimalPlaces)
{
    HalfFloat a(1.235f);
    HalfFloat b = a.Round(2);
    float c = b;

    EXPECT_EQ(b, 1.25f);
}
