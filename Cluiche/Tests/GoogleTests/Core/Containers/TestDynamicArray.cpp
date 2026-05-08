// TestDynamicArray.cpp - Google Test unit tests for DynamicArray
//
// Tests the Dia::Core::Containers::DynamicArray template class

#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>

using namespace Dia::Core::Containers;

// Test suite for DynamicArray
class DynamicArrayTest : public ::testing::Test {
protected:
    // Common test data
    static constexpr int kTestData3[3] = {1, 2, 3};
    static constexpr int kTestData5[5] = {1, 2, 3, 4, 5};
};

// Test: Default constructor creates empty array with specified capacity
TEST(DynamicArray, DefaultConstruction_CreatesEmptyArrayWithCapacity)
{
    DynamicArray<int> array(3);

    EXPECT_EQ(array.Capacity(), 3) << "Capacity should be 3";
    EXPECT_EQ(array.Size(), 0) << "Size should be 0 (empty)";
    EXPECT_FALSE(array.IsFull()) << "Array should not be full";
    EXPECT_TRUE(array.IsEmpty()) << "Array should be empty";
}

// Test: Construction from C array
TEST(DynamicArray, ConstructFromCArray_CopiesElements)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> array(&cArray[0], 3);

    EXPECT_EQ(array.Capacity(), 3);
    EXPECT_EQ(array.Size(), 3);
    EXPECT_TRUE(array.IsFull()) << "Array should be full after construction from C array";

    // Verify elements were copied correctly
    EXPECT_EQ(array.At(0), 1);
    EXPECT_EQ(array.At(1), 2);
    EXPECT_EQ(array.At(2), 3);
}

// Test: Construction from C array with more elements than capacity
TEST(DynamicArray, ConstructFromLargerCArray_OnlyCopiesUpToCapacity)
{
    int cArray[5] = {1, 2, 3, 4, 5};
    DynamicArray<int> array(&cArray[0], 3);  // Only copy first 3 elements

    EXPECT_EQ(array.Capacity(), 3);
    EXPECT_EQ(array.Size(), 3);
    EXPECT_TRUE(array.IsFull());

    // Only first 3 elements should be copied
    EXPECT_EQ(array.At(0), 1);
    EXPECT_EQ(array.At(1), 2);
    EXPECT_EQ(array.At(2), 3);
}

// Test: Copy constructor
TEST(DynamicArray, CopyConstructor_CreatesIndependentCopy)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> original(&cArray[0], 3);
    DynamicArray<int> copy(original);

    EXPECT_EQ(copy.Capacity(), 3);
    EXPECT_EQ(copy.Size(), 3);
    EXPECT_TRUE(copy.IsFull());

    // Verify all elements were copied
    EXPECT_EQ(copy.At(0), 1);
    EXPECT_EQ(copy.At(1), 2);
    EXPECT_EQ(copy.At(2), 3);

    // Modify original to ensure copy is independent
    const_cast<int&>(original.At(0)) = 99;
    EXPECT_EQ(copy.At(0), 1) << "Copy should be independent of original";
}

// Test: Add single element
TEST(DynamicArray, Add_AppendsElementAndIncreasesSize)
{
    DynamicArray<int> array(5);
    EXPECT_EQ(array.Size(), 0);

    array.Add(10);
    EXPECT_EQ(array.Size(), 1);
    EXPECT_EQ(array.At(0), 10);

    array.Add(20);
    EXPECT_EQ(array.Size(), 2);
    EXPECT_EQ(array.At(1), 20);
}

// Test: Add until full
TEST(DynamicArray, Add_FillsArrayToCapacity)
{
    DynamicArray<int> array(3);

    array.Add(1);
    array.Add(2);
    array.Add(3);

    EXPECT_TRUE(array.IsFull());
    EXPECT_EQ(array.Size(), 3);
    EXPECT_EQ(array.Capacity(), 3);
}

// Test: Remove element
TEST(DynamicArray, Remove_DecreasesSize)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> array(&cArray[0], 3);

    array.Remove();
    EXPECT_EQ(array.Size(), 2);
    EXPECT_FALSE(array.IsFull());

    array.Remove();
    EXPECT_EQ(array.Size(), 1);

    array.Remove();
    EXPECT_EQ(array.Size(), 0);
    EXPECT_TRUE(array.IsEmpty());
}

// Test: RemoveAll empties array
TEST(DynamicArray, RemoveAll_EmptiesArrayButKeepsCapacity)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> array(&cArray[0], 3);

    EXPECT_EQ(array.Size(), 3);

    array.RemoveAll();

    EXPECT_EQ(array.Size(), 0);
    EXPECT_TRUE(array.IsEmpty());
    EXPECT_EQ(array.Capacity(), 3) << "Capacity should remain unchanged after RemoveAll()";
}

// Test: Operator[] access
TEST(DynamicArray, OperatorBracket_AccessesElements)
{
    int cArray[3] = {10, 20, 30};
    DynamicArray<int> array(&cArray[0], 3);

    EXPECT_EQ(array[0], 10);
    EXPECT_EQ(array[1], 20);
    EXPECT_EQ(array[2], 30);
}

// Death test: Out of bounds access should assert in debug
#ifdef DEBUG
TEST(DynamicArrayDeathTest, At_OutOfBounds_Asserts)
{
    DynamicArray<int> array(3);
    array.Add(1);

    // Access beyond size should assert
    EXPECT_DEATH({
        volatile int x = array.At(1);  // Size is 1, so index 1 is out of bounds
        (void)x;
    }, "");  // Match any assertion message
}
#endif

// Test: Assignment operator
TEST(DynamicArray, AssignmentOperator_CopiesElements)
{
    int cArray[3] = {1, 2, 3};
    DynamicArray<int> source(&cArray[0], 3);
    DynamicArray<int> dest(5);  // Different capacity

    dest = source;

    EXPECT_EQ(dest.Size(), 3);
    EXPECT_EQ(dest.At(0), 1);
    EXPECT_EQ(dest.At(1), 2);
    EXPECT_EQ(dest.At(2), 3);
}
