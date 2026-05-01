#include <gtest/gtest.h>
#include <DiaCore/Core/MetaLogic.h>

using namespace Dia::Core;

TEST(MetaLogic, MetaOr_LogicCombinations)
{
    bool or1 = MetaOr<true, true>::value;
    bool or2 = MetaOr<true, false>::value;
    bool or3 = MetaOr<false, false>::value;

    EXPECT_TRUE(or1);
    EXPECT_TRUE(or2);
    EXPECT_FALSE(or3);
}

TEST(MetaLogic, MetaAnd_LogicCombinations)
{
    bool and1 = MetaAnd<true, true>::value;
    bool and2 = MetaAnd<true, false>::value;
    bool and3 = MetaAnd<false, false>::value;

    EXPECT_TRUE(and1);
    EXPECT_FALSE(and2);
    EXPECT_FALSE(and3);
}

TEST(MetaLogic, MetaNot_Negation)
{
    bool not1 = MetaNot<true>::value;
    bool not2 = MetaNot<false>::value;

    EXPECT_FALSE(not1);
    EXPECT_TRUE(not2);
}

TEST(MetaLogic, MetaMax_ReturnsLargerValue)
{
    int max1 = MetaMax<int, 3, 4>::value;

    EXPECT_EQ(max1, 4);
}

TEST(MetaLogic, MetaMin_ReturnsSmallerValue)
{
    int min1 = MetaMin<int, 3, 4>::value;

    EXPECT_EQ(min1, 3);
}
