#include <gtest/gtest.h>
#include <DiaMaths/Shape/Common/IntersectionClassify.h>

using Dia::Maths::IntersectionClassify;

TEST(IntersectionClassify, DefaultConstruction_IsNotIntersecting)
{
    IntersectionClassify classify1;

    EXPECT_FALSE(classify1.IsIntersecting());
    EXPECT_TRUE(classify1.IsNotIntersecting());
    EXPECT_FALSE(classify1.IsContainment());
    EXPECT_TRUE(classify1.IsNotContainment());
}

TEST(IntersectionClassify, ConstructWithClassification_SetsState)
{
    IntersectionClassify classify1(IntersectionClassify::kNoIntersection);
    IntersectionClassify classify2(IntersectionClassify::kPenatrating);
    IntersectionClassify classify3(IntersectionClassify::kAContainsB);
    IntersectionClassify classify4(IntersectionClassify::kBContainsA);

    EXPECT_FALSE(classify1.IsIntersecting());
    EXPECT_TRUE(classify1.IsNotIntersecting());
    EXPECT_FALSE(classify1.IsContainment());
    EXPECT_TRUE(classify1.IsNotContainment());

    EXPECT_TRUE(classify2.IsIntersecting());
    EXPECT_FALSE(classify2.IsNotIntersecting());
    EXPECT_FALSE(classify2.IsContainment());
    EXPECT_TRUE(classify2.IsNotContainment());

    EXPECT_TRUE(classify3.IsIntersecting());
    EXPECT_FALSE(classify3.IsNotIntersecting());
    EXPECT_TRUE(classify3.IsContainment());
    EXPECT_FALSE(classify3.IsNotContainment());

    EXPECT_TRUE(classify4.IsIntersecting());
    EXPECT_FALSE(classify4.IsNotIntersecting());
    EXPECT_TRUE(classify4.IsContainment());
    EXPECT_FALSE(classify4.IsNotContainment());
}

TEST(IntersectionClassify, SetClassification_UpdatesState)
{
    IntersectionClassify classify1;
    classify1.SetClassification(IntersectionClassify::kNoIntersection);
    IntersectionClassify classify2;
    classify2.SetClassification(IntersectionClassify::kPenatrating);
    IntersectionClassify classify3;
    classify3.SetClassification(IntersectionClassify::kAContainsB);
    IntersectionClassify classify4;
    classify4.SetClassification(IntersectionClassify::kBContainsA);

    EXPECT_EQ(classify1.GetClassification(), IntersectionClassify::kNoIntersection);
    EXPECT_EQ(classify2.GetClassification(), IntersectionClassify::kPenatrating);
    EXPECT_EQ(classify3.GetClassification(), IntersectionClassify::kAContainsB);
    EXPECT_EQ(classify4.GetClassification(), IntersectionClassify::kBContainsA);

    EXPECT_FALSE(classify1.IsIntersecting());
    EXPECT_TRUE(classify1.IsNotIntersecting());
    EXPECT_FALSE(classify1.IsContainment());
    EXPECT_TRUE(classify1.IsNotContainment());

    EXPECT_TRUE(classify2.IsIntersecting());
    EXPECT_FALSE(classify2.IsNotIntersecting());
    EXPECT_FALSE(classify2.IsContainment());
    EXPECT_TRUE(classify2.IsNotContainment());

    EXPECT_TRUE(classify3.IsIntersecting());
    EXPECT_FALSE(classify3.IsNotIntersecting());
    EXPECT_TRUE(classify3.IsContainment());
    EXPECT_FALSE(classify3.IsNotContainment());

    EXPECT_TRUE(classify4.IsIntersecting());
    EXPECT_FALSE(classify4.IsNotIntersecting());
    EXPECT_TRUE(classify4.IsContainment());
    EXPECT_FALSE(classify4.IsNotContainment());
}

TEST(IntersectionClassify, AssignmentAndComparison_WorkCorrectly)
{
    IntersectionClassify classify1(IntersectionClassify::kNoIntersection);
    IntersectionClassify classify2(IntersectionClassify::kPenatrating);
    IntersectionClassify classify3;

    classify3 = classify2;

    EXPECT_NE(classify1, classify2);
    EXPECT_NE(classify1, classify3);
    EXPECT_EQ(classify3, classify2);
}

TEST(IntersectionClassify, ReInterpretAandBObject_SwapsContainment)
{
    IntersectionClassify classify1(IntersectionClassify::kAContainsB);
    IntersectionClassify classify2(IntersectionClassify::kBContainsA);

    IntersectionClassify classify3(IntersectionClassify::kAContainsB);
    IntersectionClassify classify4(IntersectionClassify::kBContainsA);

    classify2.ReInterpretAandBObject();
    classify3.ReInterpretAandBObject();

    EXPECT_EQ(classify1, classify2);
    EXPECT_EQ(classify3, classify4);
}
