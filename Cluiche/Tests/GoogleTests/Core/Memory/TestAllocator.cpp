// TestAllocator.cpp - Google Test unit tests for Allocator helpers
//
// Tests AllocatorStats, AlignUp, AlignDown, IsAligned, and AlignPointer
// from DiaCore Memory subsystem

#include <gtest/gtest.h>
#include <DiaCore/Memory/IAllocator.h>

using namespace Dia::Core;

// ==============================================================================
// AllocatorStats Tests
// ==============================================================================

TEST(Allocator, AllocatorStats_DefaultConstruction_AllFieldsZero)
{
    AllocatorStats stats;

    EXPECT_EQ(stats.totalAllocated, 0u);
    EXPECT_EQ(stats.totalUsed, 0u);
    EXPECT_EQ(stats.allocationCount, 0u);
    EXPECT_EQ(stats.peakUsed, 0u);
    EXPECT_EQ(stats.totalAllocations, 0u);
    EXPECT_EQ(stats.totalDeallocations, 0u);
}

// ==============================================================================
// AlignUp Tests
// ==============================================================================

TEST(Allocator, AlignUp_AlreadyAligned_ReturnsSameValue)
{
    EXPECT_EQ(AlignUp(8, 4), 8u);
    EXPECT_EQ(AlignUp(16, 8), 16u);
    EXPECT_EQ(AlignUp(64, 16), 64u);
}

TEST(Allocator, AlignUp_UnalignedValue_RoundsUp)
{
    EXPECT_EQ(AlignUp(5, 4), 8u);
    EXPECT_EQ(AlignUp(1, 4), 4u);
    EXPECT_EQ(AlignUp(9, 8), 16u);
    EXPECT_EQ(AlignUp(15, 16), 16u);
}

TEST(Allocator, AlignUp_ZeroValue_ReturnsZero)
{
    EXPECT_EQ(AlignUp(0, 4), 0u);
    EXPECT_EQ(AlignUp(0, 8), 0u);
    EXPECT_EQ(AlignUp(0, 16), 0u);
}

TEST(Allocator, AlignUp_PowerOfTwoAlignments_CorrectResults)
{
    // Alignment 4
    EXPECT_EQ(AlignUp(3, 4), 4u);
    EXPECT_EQ(AlignUp(4, 4), 4u);
    EXPECT_EQ(AlignUp(7, 4), 8u);

    // Alignment 8
    EXPECT_EQ(AlignUp(1, 8), 8u);
    EXPECT_EQ(AlignUp(8, 8), 8u);
    EXPECT_EQ(AlignUp(13, 8), 16u);

    // Alignment 16
    EXPECT_EQ(AlignUp(1, 16), 16u);
    EXPECT_EQ(AlignUp(16, 16), 16u);
    EXPECT_EQ(AlignUp(17, 16), 32u);

    // Alignment 64
    EXPECT_EQ(AlignUp(1, 64), 64u);
    EXPECT_EQ(AlignUp(64, 64), 64u);
    EXPECT_EQ(AlignUp(65, 64), 128u);
}

// ==============================================================================
// AlignDown Tests
// ==============================================================================

TEST(Allocator, AlignDown_AlreadyAligned_ReturnsSameValue)
{
    EXPECT_EQ(AlignDown(8, 4), 8u);
    EXPECT_EQ(AlignDown(16, 8), 16u);
    EXPECT_EQ(AlignDown(64, 16), 64u);
}

TEST(Allocator, AlignDown_UnalignedValue_RoundsDown)
{
    EXPECT_EQ(AlignDown(5, 4), 4u);
    EXPECT_EQ(AlignDown(7, 4), 4u);
    EXPECT_EQ(AlignDown(9, 8), 8u);
    EXPECT_EQ(AlignDown(15, 16), 0u);
    EXPECT_EQ(AlignDown(31, 16), 16u);
}

TEST(Allocator, AlignDown_ZeroValue_ReturnsZero)
{
    EXPECT_EQ(AlignDown(0, 4), 0u);
    EXPECT_EQ(AlignDown(0, 8), 0u);
    EXPECT_EQ(AlignDown(0, 16), 0u);
}

// ==============================================================================
// IsAligned Tests
// ==============================================================================

TEST(Allocator, IsAligned_AlignedValue_ReturnsTrue)
{
    EXPECT_TRUE(IsAligned(0, 4));
    EXPECT_TRUE(IsAligned(4, 4));
    EXPECT_TRUE(IsAligned(8, 4));
    EXPECT_TRUE(IsAligned(16, 8));
    EXPECT_TRUE(IsAligned(64, 16));
    EXPECT_TRUE(IsAligned(128, 64));
}

TEST(Allocator, IsAligned_UnalignedValue_ReturnsFalse)
{
    EXPECT_FALSE(IsAligned(1, 4));
    EXPECT_FALSE(IsAligned(3, 4));
    EXPECT_FALSE(IsAligned(5, 8));
    EXPECT_FALSE(IsAligned(7, 16));
    EXPECT_FALSE(IsAligned(33, 64));
}

// ==============================================================================
// AlignPointer Tests
// ==============================================================================

TEST(Allocator, AlignPointer_ReturnsAlignedPointer)
{
    // Create a buffer large enough to have both aligned and unaligned addresses
    alignas(64) char buffer[128];

    // Get a deliberately unaligned pointer by offsetting 1 byte
    void* unaligned = static_cast<void*>(buffer + 1);

    void* aligned = AlignPointer(unaligned, 16);

    // The result must be >= the input (we align up, not down)
    EXPECT_GE(reinterpret_cast<size_t>(aligned), reinterpret_cast<size_t>(unaligned));

    // The result must be aligned to 16
    EXPECT_TRUE(IsAligned(reinterpret_cast<size_t>(aligned), 16));
}
