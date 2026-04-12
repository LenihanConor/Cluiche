#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/ArrayC.h>

using namespace Dia::Core::Containers;

TEST(ArrayIterators, ConstIterator_NavigationAndComparison)
{
    int cArray1[3] = {1, 2, 3};
    ArrayC<int, 3> tempArray(cArray1, 3);

    ArrayC<int, 3>::ConstIterator iter1(tempArray.Begin());
    ArrayC<int, 3>::ConstIterator iter2(tempArray.Begin());

    EXPECT_EQ(iter1.Begin(), &tempArray[0]);
    EXPECT_EQ(iter1.Begin(), iter1.Current());
    EXPECT_EQ(*iter1.Begin(), 1);

    EXPECT_EQ(iter1.End(), &tempArray[2]);
    EXPECT_EQ(*iter1.End(), 3);

    EXPECT_FALSE(iter1.IsDone());
    EXPECT_EQ(iter1, iter2);
    EXPECT_LE(iter1, iter2);
    EXPECT_GE(iter1, iter2);

    EXPECT_FALSE(iter1 != iter2);
    EXPECT_FALSE(iter1 > iter2);
    EXPECT_FALSE(iter1 < iter2);

    iter2.Next();

    EXPECT_FALSE(iter2.IsDone());
    EXPECT_EQ(iter2.Current(), &tempArray[1]);
    EXPECT_EQ(*iter2.Current(), 2);

    EXPECT_NE(iter1, iter2);
    EXPECT_LT(iter1, iter2);
    EXPECT_LE(iter1, iter2);

    EXPECT_FALSE(iter1 == iter2);
    EXPECT_FALSE(iter1 > iter2);
    EXPECT_FALSE(iter1 >= iter2);

    iter2.Next();

    EXPECT_FALSE(iter2.IsDone());
    EXPECT_EQ(iter2.Current(), &tempArray[2]);
    EXPECT_EQ(*iter2.Current(), 3);
    EXPECT_EQ(iter2.Current(), iter2.End());

    iter2.Next();

    EXPECT_TRUE(iter2.IsDone());

    EXPECT_DEATH({ iter2.Current(); }, "");

    iter2.Previous();

    EXPECT_FALSE(iter2.IsDone());
    EXPECT_EQ(iter2.Current(), &tempArray[2]);
    EXPECT_EQ(*iter2.Current(), 3);
    EXPECT_EQ(iter2.Current(), iter2.End());

    iter1.Previous();

    EXPECT_TRUE(iter1.IsDone());

    EXPECT_DEATH({ iter1.Current(); }, "");
}

TEST(ArrayIterators, ConstReverseIterator_NavigationAndComparison)
{
    int cArray1[3] = {1, 2, 3};
    ArrayC<int, 3> tempArray(cArray1, 3);

    ArrayC<int, 3>::ConstReverseIterator iter1(tempArray.End());
    ArrayC<int, 3>::ConstReverseIterator iter2(tempArray.End());
    ArrayC<int, 3>::ConstReverseIterator iter3(tempArray.End());

    EXPECT_EQ(iter1.Begin(), &tempArray[2]);
    EXPECT_EQ(iter1.Begin(), iter1.Current());
    EXPECT_EQ(*iter1.Begin(), 3);

    EXPECT_EQ(iter1.End(), &tempArray[0]);
    EXPECT_EQ(*iter1.End(), 1);

    EXPECT_FALSE(iter1.IsDone());
    EXPECT_EQ(iter1, iter2);
    EXPECT_LE(iter1, iter2);
    EXPECT_GE(iter1, iter2);

    EXPECT_FALSE(iter1 != iter2);
    EXPECT_FALSE(iter1 > iter2);
    EXPECT_FALSE(iter1 < iter2);

    iter2.Next();

    EXPECT_FALSE(iter2.IsDone());
    EXPECT_EQ(iter2.Current(), &tempArray[1]);
    EXPECT_EQ(*iter2.Current(), 2);

    EXPECT_NE(iter1, iter2);
    EXPECT_LT(iter1, iter2);
    EXPECT_LE(iter1, iter2);

    EXPECT_FALSE(iter1 == iter2);
    EXPECT_FALSE(iter1 > iter2);
    EXPECT_FALSE(iter1 >= iter2);

    iter2.Next();

    EXPECT_FALSE(iter2.IsDone());
    EXPECT_EQ(iter2.Current(), &tempArray[0]);
    EXPECT_EQ(*iter2.Current(), 1);
    EXPECT_EQ(iter2.Current(), iter2.End());

    iter2.Next();

    EXPECT_TRUE(iter2.IsDone());

    EXPECT_DEATH({ iter2.Current(); }, "");

    iter2.Previous();

    EXPECT_FALSE(iter2.IsDone());
    EXPECT_EQ(iter2.Current(), &tempArray[0]);
    EXPECT_EQ(*iter2.Current(), 1);
    EXPECT_EQ(iter2.Current(), iter2.End());

    iter1.Previous();

    EXPECT_TRUE(iter1.IsDone());

    EXPECT_DEATH({ iter1.Current(); }, "");
}
