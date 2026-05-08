#include <gtest/gtest.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTableC.h>
#include <random>

using namespace Dia::Core::Containers;

struct ModHashFunctor
{
    unsigned int GetHashIndex(unsigned int key,
                              const HashTableHashFunctionData* data) const
    {
        return key % data->GetTableSize();
    }
};

static std::mt19937 MakeRng() { return std::mt19937(0xABCD5678u); }

// ---------------------------------------------------------------------------
// DynamicArrayC invariants
// ---------------------------------------------------------------------------

TEST(DiaCore_ContainerInvariants, DynamicArray_SizeNeverExceedsCapacity)
{
    auto rng = MakeRng();
    std::uniform_int_distribution<int> opDist(0, 1);

    DynamicArrayC<int, 256> arr;

    for (int op = 0; op < 1000; ++op)
    {
        int action = opDist(rng);
        if (action == 0 && !arr.IsFull())
        {
            arr.Add(op);
        }
        else if (action == 1 && !arr.IsEmpty())
        {
            arr.Remove();
        }
        EXPECT_LE(arr.Size(), arr.Capacity())
            << "Size exceeded capacity after op " << op;
    }
}

TEST(DiaCore_ContainerInvariants, DynamicArray_SizeMathConsistent)
{
    // Size == number of adds - number of removes
    DynamicArrayC<int, 512> arr;

    int addCount    = 0;
    int removeCount = 0;

    for (int i = 0; i < 200; ++i)
    {
        arr.Add(i);
        ++addCount;
    }
    for (int i = 0; i < 50; ++i)
    {
        arr.Remove();
        ++removeCount;
    }
    for (int i = 0; i < 80; ++i)
    {
        arr.Add(i + 1000);
        ++addCount;
    }

    EXPECT_EQ(arr.Size(), static_cast<unsigned int>(addCount - removeCount));
}

TEST(DiaCore_ContainerInvariants, DynamicArray_BackAlwaysLastAdded)
{
    DynamicArrayC<int, 64> arr;

    for (int i = 0; i < 50; ++i)
    {
        arr.Add(i * 7);
        EXPECT_EQ(arr.Back(), i * 7)
            << "Back() should equal last added at i=" << i;
    }
}

TEST(DiaCore_ContainerInvariants, DynamicArray_FrontStableUnlessRemoved)
{
    DynamicArrayC<int, 64> arr;
    arr.Add(42);
    arr.Add(100);
    arr.Add(200);

    EXPECT_EQ(arr.Front(), 42);

    arr.Remove();   // removes Back (200), not Front
    EXPECT_EQ(arr.Front(), 42) << "Front should be stable after Remove() from back";
}

// ---------------------------------------------------------------------------
// HashTableC invariants
// ---------------------------------------------------------------------------

TEST(DiaCore_ContainerInvariants, HashTable_AllInsertedKeysRetrievable)
{
    HashTableC<unsigned int, int, ModHashFunctor, 128, 131> table;

    for (unsigned int i = 0; i < 100; ++i)
    {
        table.Add(i, static_cast<int>(i * 3));
    }

    EXPECT_EQ(table.Size(), 100u);

    for (unsigned int i = 0; i < 100; ++i)
    {
        EXPECT_TRUE(table.ContainsKey(i))     << "Key " << i << " missing";
        EXPECT_EQ(table.GetItemConst(i), static_cast<int>(i * 3)) << "Wrong value for key " << i;
    }
}

TEST(DiaCore_ContainerInvariants, HashTable_SizeMatchesInsertCount)
{
    HashTableC<unsigned int, int, ModHashFunctor, 64, 67> table;

    for (unsigned int i = 0; i < 60; ++i)
    {
        table.Add(i, static_cast<int>(i));
        EXPECT_EQ(table.Size(), i + 1) << "Size mismatch after inserting key " << i;
    }
}

TEST(DiaCore_ContainerInvariants, HashTable_RemovedKeyAbsentFromTable)
{
    HashTableC<unsigned int, int, ModHashFunctor, 64, 67> table;

    for (unsigned int i = 0; i < 30; ++i)
        table.Add(i, static_cast<int>(i));

    // Remove keys in reverse order (safe for DynamicArrayC backing)
    for (unsigned int i = 29; i >= 20; --i)
        table.Remove(i);

    for (unsigned int i = 20; i < 30; ++i)
        EXPECT_FALSE(table.ContainsKey(i)) << "Removed key " << i << " still present";

    for (unsigned int i = 0; i < 20; ++i)
        EXPECT_TRUE(table.ContainsKey(i))  << "Non-removed key " << i << " missing";
}
