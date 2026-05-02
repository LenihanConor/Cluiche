// TestContainerStress.cpp - Stress and boundary tests for DiaCore containers
//
// Covers DynamicArrayC fill-to-capacity, add/remove cycles, and boundary asserts,
// plus HashTableC bulk-insert, insert/remove cycles, and near-capacity behaviour.

#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTableC.h>

using namespace Dia::Core::Containers;

// ---------------------------------------------------------------------------
// Shared hash functor used by all HashTableC stress tests.
// Keys are unsigned ints mapped directly to table slots (mod table size).
// ---------------------------------------------------------------------------
struct UIntModHashFunctor
{
    unsigned int GetHashIndex(unsigned int key,
                              const HashTableHashFunctionData* data) const
    {
        return key % data->GetTableSize();
    }
};

// ===========================================================================
// DynamicArrayC stress tests
// ===========================================================================

// Fill a DynamicArrayC<int,1000> with 1000 elements and verify every value.
TEST(DiaCore_ContainerStress, DynamicArrayC_FillToCapacity_SizeAndValuesCorrect)
{
    DynamicArrayC<int, 1000> array;

    for (int i = 0; i < 1000; ++i)
    {
        array.Add(i);
    }

    EXPECT_EQ(array.Size(), 1000u);
    EXPECT_TRUE(array.IsFull());

    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_EQ(array.At(static_cast<unsigned int>(i)), i);
    }
}

// Rapid add/remove cycle: add 500, remove all, add 500 again — size stays correct.
TEST(DiaCore_ContainerStress, DynamicArrayC_AddRemoveCycle_SizeCorrectAfterEachPhase)
{
    DynamicArrayC<int, 1000> array;

    for (int i = 0; i < 500; ++i)
    {
        array.Add(i * 2);
    }
    EXPECT_EQ(array.Size(), 500u);

    array.RemoveAll();
    EXPECT_EQ(array.Size(), 0u);
    EXPECT_TRUE(array.IsEmpty());

    for (int i = 0; i < 500; ++i)
    {
        array.Add(i * 3);
    }
    EXPECT_EQ(array.Size(), 500u);
    EXPECT_EQ(array.Front(), 0);
    EXPECT_EQ(array.Back(), 499 * 3);
}

// Remove() strips the last element one at a time; size decrements correctly.
TEST(DiaCore_ContainerStress, DynamicArrayC_RemoveLastRepeatedly_SizeDecrementsCorrectly)
{
    static const unsigned int kCount = 200;
    DynamicArrayC<int, 512> array;

    for (unsigned int i = 0; i < kCount; ++i)
    {
        array.Add(static_cast<int>(i));
    }

    for (unsigned int i = kCount; i > 0; --i)
    {
        EXPECT_EQ(array.Size(), i);
        array.Remove();   // removes last element
    }

    EXPECT_EQ(array.Size(), 0u);
    EXPECT_TRUE(array.IsEmpty());
}

// Verify that Add() beyond capacity fires an assertion (DEBUG build only).
TEST(DiaCore_ContainerStress, DynamicArrayC_AddBeyondCapacity_AssertsInDebug)
{
#ifdef DEBUG
    DynamicArrayC<int, 4> array;
    array.Add(1);
    array.Add(2);
    array.Add(3);
    array.Add(4);
    EXPECT_TRUE(array.IsFull());
    EXPECT_DEATH({ array.Add(5); }, "");
#else
    GTEST_SKIP() << "Capacity-overflow assert only fires in DEBUG builds";
#endif
}

// Verify that operator[](int -1) asserts (signed-index guard).
TEST(DiaCore_ContainerStress, DynamicArrayC_NegativeIndex_AssertsInDebug)
{
#ifdef DEBUG
    DynamicArrayC<int, 8> array;
    array.Add(42);
    EXPECT_DEATH({ int val = array[-1]; (void)val; }, "");
#else
    GTEST_SKIP() << "Negative-index assert only fires in DEBUG builds";
#endif
}

// Verify that At(Size()) — one past the last valid index — asserts.
TEST(DiaCore_ContainerStress, DynamicArrayC_AccessAtSizeIndex_AssertsInDebug)
{
#ifdef DEBUG
    DynamicArrayC<int, 8> array;
    array.Add(10);
    array.Add(20);
    array.Add(30);
    // Size() == 3; At(3) is one past the end.
    EXPECT_DEATH({ int val = array.At(array.Size()); (void)val; }, "");
#else
    GTEST_SKIP() << "Out-of-bounds assert only fires in DEBUG builds";
#endif
}

// ===========================================================================
// HashTableC stress tests
// ===========================================================================

// Insert 200 unique keys and verify every key is retrievable via ContainsKey.
TEST(DiaCore_ContainerStress, HashTableC_Insert200UniqueKeys_AllRetrievable)
{
    // sizePayload=200, sizeTable=211 (prime > 200 for good distribution)
    HashTableC<unsigned int, int, UIntModHashFunctor, 200, 211> table;

    EXPECT_TRUE(table.IsEmpty());

    for (unsigned int i = 0; i < 200; ++i)
    {
        table.Add(i, static_cast<int>(i * 10));
    }

    EXPECT_EQ(table.Size(), 200u);
    EXPECT_TRUE(table.IsFull());

    for (unsigned int i = 0; i < 200; ++i)
    {
        EXPECT_TRUE(table.ContainsKey(i));
        EXPECT_EQ(table.GetItemConst(i), static_cast<int>(i * 10));
    }
}

// Insert 100 items, remove 50 from the tail (reverse insertion order), insert 50 new ones.
//
// HashTableC::Remove uses RemoveAt on a DynamicArrayC which compacts the backing array.
// Removing in reverse insertion order means each removal targets the last slot — no shift
// occurs and mTable/mNext pointers remain valid.
TEST(DiaCore_ContainerStress, HashTableC_InsertRemoveCycle_SizeRemainsCorrect)
{
    HashTableC<unsigned int, int, UIntModHashFunctor, 150, 151> table;

    for (unsigned int i = 0; i < 100; ++i)
    {
        table.Add(i, static_cast<int>(i));
    }
    EXPECT_EQ(table.Size(), 100u);

    // Remove the last 50 keys in reverse order (99 down to 50).
    // Each removal targets the current tail of mPayloadNodes — no index shift.
    for (unsigned int i = 99; i >= 50; --i)
    {
        table.Remove(i);
    }
    EXPECT_EQ(table.Size(), 50u);

    // Confirm removed keys are gone, remaining keys still present
    for (unsigned int i = 50; i < 100; ++i)
    {
        EXPECT_FALSE(table.ContainsKey(i));
    }
    for (unsigned int i = 0; i < 50; ++i)
    {
        EXPECT_TRUE(table.ContainsKey(i));
    }

    // Insert 50 new keys (1000..1049) to restore size to 100
    for (unsigned int i = 0; i < 50; ++i)
    {
        table.Add(1000u + i, static_cast<int>(1000 + i));
    }
    EXPECT_EQ(table.Size(), 100u);

    for (unsigned int i = 0; i < 50; ++i)
    {
        EXPECT_TRUE(table.ContainsKey(1000u + i));
    }
}

// Fill to near-capacity (one slot free) and verify no crash or data corruption.
TEST(DiaCore_ContainerStress, HashTableC_FillToNearCapacity_NoCrashAndDataIntact)
{
    static const unsigned int kCapacity = 64;
    static const unsigned int kFill     = kCapacity - 1;   // one slot free

    HashTableC<unsigned int, int, UIntModHashFunctor, kCapacity, 67> table;

    for (unsigned int i = 0; i < kFill; ++i)
    {
        table.Add(i, static_cast<int>(i + 1));
    }

    EXPECT_EQ(table.Size(), kFill);
    EXPECT_FALSE(table.IsEmpty());
    EXPECT_FALSE(table.IsFull());

    // Spot-check a few values
    EXPECT_EQ(table.GetItemConst(0u),          1);
    EXPECT_EQ(table.GetItemConst(kFill - 1u),  static_cast<int>(kFill));
}

// RemoveAll() on a HashTableC resets size to zero.
TEST(DiaCore_ContainerStress, HashTableC_RemoveAll_SizeBecomesZero)
{
    HashTableC<unsigned int, int, UIntModHashFunctor, 50, 53> table;

    for (unsigned int i = 0; i < 50; ++i)
    {
        table.Add(i, static_cast<int>(i));
    }
    EXPECT_TRUE(table.IsFull());

    table.RemoveAll();

    EXPECT_EQ(table.Size(), 0u);
    EXPECT_TRUE(table.IsEmpty());
}
