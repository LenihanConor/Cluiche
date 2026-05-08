#include <gtest/gtest.h>
#include <DiaCore/Containers/Handle.h>

using HandleType = Dia::Core::Handle<int>;

// ---------------------------------------------------------------------------
// DiaCore Handle<T>
// ---------------------------------------------------------------------------

TEST(DiaCore_Handle, DefaultConstruction_IsNotValid)
{
    HandleType h;
    EXPECT_FALSE(h.IsValid());
}

TEST(DiaCore_Handle, ValidHandle_IsValid)
{
    // Index=0 and generation=1 → valid (index != kInvalidIndex and gen != kInvalidGeneration).
    HandleType h(0u, 1u);
    EXPECT_TRUE(h.IsValid());
}

TEST(DiaCore_Handle, Invalid_StaticFactory_IsNotValid)
{
    HandleType h = HandleType::Invalid();
    EXPECT_FALSE(h.IsValid());
}

TEST(DiaCore_Handle, GetIndex_ReturnsCorrectIndex)
{
    HandleType h(7u, 3u);
    EXPECT_EQ(h.GetIndex(), 7u);
}

TEST(DiaCore_Handle, GetGeneration_ReturnsCorrectGeneration)
{
    HandleType h(7u, 3u);
    EXPECT_EQ(h.GetGeneration(), 3u);
}

TEST(DiaCore_Handle, EqualityOperator_SameValues_Equal)
{
    HandleType a(5u, 2u);
    HandleType b(5u, 2u);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(DiaCore_Handle, EqualityOperator_DifferentIndex_NotEqual)
{
    HandleType a(5u, 2u);
    HandleType b(6u, 2u);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(DiaCore_Handle, EqualityOperator_DifferentGeneration_NotEqual)
{
    HandleType a(5u, 2u);
    HandleType b(5u, 3u);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(DiaCore_Handle, DefaultHandle_IndexAndGenerationAreInvalidSentinels)
{
    HandleType h;
    EXPECT_EQ(h.GetIndex(),      HandleType::kInvalidIndex);
    EXPECT_EQ(h.GetGeneration(), HandleType::kInvalidGeneration);
}

TEST(DiaCore_Handle, InvalidFactory_MatchesDefaultConstruction)
{
    HandleType defaultH;
    HandleType invalidH = HandleType::Invalid();
    EXPECT_EQ(defaultH, invalidH);
}

TEST(DiaCore_Handle, HandleWithZeroGeneration_IsNotValid)
{
    // Generation=0 is the kInvalidGeneration sentinel → should not be valid.
    HandleType h(0u, 0u);
    EXPECT_FALSE(h.IsValid());
}

TEST(DiaCore_Handle, HandleWithMaxIndex_IsNotValid)
{
    // Index=0xFFFFFFFF is kInvalidIndex → should not be valid regardless of generation.
    HandleType h(HandleType::kInvalidIndex, 1u);
    EXPECT_FALSE(h.IsValid());
}

TEST(DiaCore_Handle, HandleWithValidIndexAndValidGeneration_IsValid)
{
    HandleType h(0u, 1u);
    EXPECT_TRUE(h.IsValid());
}

TEST(DiaCore_Handle, DifferentT_SameValues_AreIndependentTypes)
{
    // Handles of different T should not compare across types —
    // but we can at least confirm same-type handles work correctly.
    Dia::Core::Handle<float> hf(3u, 1u);
    Dia::Core::Handle<int>   hi(3u, 1u);
    // They are distinct C++ types; just confirm each reports IsValid correctly.
    EXPECT_TRUE(hf.IsValid());
    EXPECT_TRUE(hi.IsValid());
}

TEST(DiaCore_Handle, CopyConstruction_PreservesValues)
{
    HandleType original(10u, 5u);
    HandleType copy(original);
    EXPECT_EQ(copy, original);
    EXPECT_TRUE(copy.IsValid());
}

TEST(DiaCore_Handle, AssignmentOperator_PreservesValues)
{
    HandleType a(10u, 5u);
    HandleType b;
    b = a;
    EXPECT_EQ(b, a);
    EXPECT_TRUE(b.IsValid());
}
