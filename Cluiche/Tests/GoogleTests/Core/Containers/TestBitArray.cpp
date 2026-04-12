#include <gtest/gtest.h>
#include <DiaCore/Containers/BitFlag/BitArray8.h>
#include <DiaCore/Containers/BitFlag/BitArray16.h>
#include <DiaCore/Containers/BitFlag/BitArray32.h>
#include <DiaCore/Containers/BitFlag/BitArray64.h>

using namespace Dia::Core;

// ============================================================================
// BitArray8 Tests
// ============================================================================

TEST(BitArray8, DefaultConstructor_IsAllOff)
{
    BitArray8 bitflag;

    EXPECT_EQ(sizeof(bitflag), 1);
    EXPECT_TRUE(bitflag.IsAllOff());
    EXPECT_FALSE(bitflag.IsAllOn());
}

TEST(BitArray8, ConstructWithZero_AllBitsOff)
{
    BitArray8 bitflag1(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray8::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray8, ConstructWithOne_FirstBitOn)
{
    BitArray8 bitflag2(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray8::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray8, ConstructWithAllBitsOn_IsAllOn)
{
    BitArray8 bitflag3(0xFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray8::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray8, ArrayAccessor_NegativeIndex_Asserts)
{
    BitArray8 bitflag3(0xFF);
    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
}

TEST(BitArray8, ArrayAccessor_OutOfBounds_Asserts)
{
    BitArray8 bitflag3(0xFF);
    EXPECT_DEATH({ bool a = bitflag3[8]; (void)a; }, "");
}

TEST(BitArray8, CopyConstructor_CopiesAllBits)
{
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(bitflag1);

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray8, AssignmentFromValue_SetsCorrectly)
{
    BitArray8 bitflag1 = 1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);
}

TEST(BitArray8, AssignmentOperator_CopiesAllBits)
{
    BitArray8 bitflag1(1);
    BitArray8 bitflag2 = bitflag1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray8, EqualityOperators_CompareCorrectly)
{
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(1);
    BitArray8 bitflag3(2);

    EXPECT_EQ(bitflag1, bitflag2);
    EXPECT_NE(bitflag1, bitflag3);
    EXPECT_NE(bitflag2, bitflag3);
}

TEST(BitArray8, Invert_FlipsAllBits)
{
    BitArray8 bitflag1;
    BitArray8 bitflag2(bitflag1);

    bitflag2.Invert();

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_TRUE(bitflag2.IsAllOn());
}

TEST(BitArray8, Clear_SetsAllBitsOff)
{
    BitArray8 bitflag1(2);

    EXPECT_FALSE(bitflag1.IsAllOff());

    bitflag1.Clear();

    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray8, SetAllBits_WithZero_AllBitsOff)
{
    BitArray8 bitflag1;
    bitflag1.SetAllBits(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray8::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray8, SetAllBits_WithOne_FirstBitOn)
{
    BitArray8 bitflag2;
    bitflag2.SetAllBits(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray8::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray8, SetAllBits_WithAllOnes_IsAllOn)
{
    BitArray8 bitflag3(0xFF);
    bitflag3.SetAllBits(0xFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray8::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray8, SetAllBits_OutOfBoundsAccess_Asserts)
{
    BitArray8 bitflag3(0xFF);
    bitflag3.SetAllBits(0xFF);

    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
    EXPECT_DEATH({ bool a = bitflag3[8]; (void)a; }, "");
}

TEST(BitArray8, SetBit_TurnsOnAndOff)
{
    BitArray8 bitflag1;

    bitflag1.SetBit(0, true);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.SetBit(0, false);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray8, ToggleBit_FlipsBitState)
{
    BitArray8 bitflag1;

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray8, GetNumberBitsSetOn_CountsCorrectly)
{
    BitArray8 bitflag1(1);
    EXPECT_EQ(bitflag1.GetNumberBitsSetOn(), 1);

    BitArray8 bitflag2(0);
    EXPECT_EQ(bitflag2.GetNumberBitsSetOn(), 0);

    BitArray8 bitflag3(0xFF);
    EXPECT_EQ(bitflag3.GetNumberBitsSetOn(), 8);
}

TEST(BitArray8, BitwiseOr_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(2);

    unsigned char bit3 = bit1 | bit2;
    BitArray8 bitflag3 = bitflag1 | bitflag2;

    EXPECT_EQ(bit3, bitflag3.GetAllBits());
}

TEST(BitArray8, BitwiseAnd_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(2);

    unsigned char bit4 = bit1 & bit2;
    BitArray8 bitflag4 = bitflag1 & bitflag2;

    EXPECT_EQ(bit4, bitflag4.GetAllBits());
}

TEST(BitArray8, BitwiseXor_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(2);

    unsigned char bit5 = bit1 ^ bit2;
    BitArray8 bitflag5 = bitflag1 ^ bitflag2;

    EXPECT_EQ(bit5, bitflag5.GetAllBits());
}

TEST(BitArray8, CompoundOrAssignment_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(2);

    bit1 |= bit2;
    bitflag1 |= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray8, CompoundAndAssignment_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(2);

    bit1 &= bit2;
    bitflag1 &= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray8, CompoundXorAssignment_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray8 bitflag1(1);
    BitArray8 bitflag2(2);

    bit1 ^= bit2;
    bitflag1 ^= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

// ============================================================================
// BitArray16 Tests
// ============================================================================

TEST(BitArray16, DefaultConstructor_IsAllOff)
{
    BitArray16 bitflag;

    EXPECT_EQ(sizeof(bitflag), 2);
    EXPECT_TRUE(bitflag.IsAllOff());
    EXPECT_FALSE(bitflag.IsAllOn());
}

TEST(BitArray16, ConstructWithZero_AllBitsOff)
{
    BitArray16 bitflag1(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray16::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray16, ConstructWithOne_FirstBitOn)
{
    BitArray16 bitflag2(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray16::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray16, ConstructWithAllBitsOn_IsAllOn)
{
    BitArray16 bitflag3(0xFFFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray16::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray16, ArrayAccessor_NegativeIndex_Asserts)
{
    BitArray16 bitflag3(0xFFFF);
    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
}

TEST(BitArray16, ArrayAccessor_OutOfBounds_Asserts)
{
    BitArray16 bitflag3(0xFFFF);
    EXPECT_DEATH({ bool a = bitflag3[17]; (void)a; }, "");
}

TEST(BitArray16, CopyConstructor_CopiesAllBits)
{
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(bitflag1);

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray16, AssignmentFromValue_SetsCorrectly)
{
    BitArray16 bitflag1 = 1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);
}

TEST(BitArray16, AssignmentOperator_CopiesAllBits)
{
    BitArray16 bitflag1(1);
    BitArray16 bitflag2 = bitflag1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray16, EqualityOperators_CompareCorrectly)
{
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(1);
    BitArray16 bitflag3(2);

    EXPECT_EQ(bitflag1, bitflag2);
    EXPECT_NE(bitflag1, bitflag3);
    EXPECT_NE(bitflag2, bitflag3);
}

TEST(BitArray16, Invert_FlipsAllBits)
{
    BitArray16 bitflag1;
    BitArray16 bitflag2(bitflag1);

    bitflag2.Invert();

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_TRUE(bitflag2.IsAllOn());
}

TEST(BitArray16, Clear_SetsAllBitsOff)
{
    BitArray16 bitflag1(2);

    EXPECT_FALSE(bitflag1.IsAllOff());

    bitflag1.Clear();

    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray16, SetAllBits_WithZero_AllBitsOff)
{
    BitArray16 bitflag1;
    bitflag1.SetAllBits(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray16::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray16, SetAllBits_WithOne_FirstBitOn)
{
    BitArray16 bitflag2;
    bitflag2.SetAllBits(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray16::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray16, SetAllBits_WithAllOnes_IsAllOn)
{
    BitArray16 bitflag3(0xFFFF);
    bitflag3.SetAllBits(0xFFFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray16::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray16, SetAllBits_OutOfBoundsAccess_Asserts)
{
    BitArray16 bitflag3(0xFFFF);
    bitflag3.SetAllBits(0xFFFF);

    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
    EXPECT_DEATH({ bool a = bitflag3[16]; (void)a; }, "");
}

TEST(BitArray16, SetBit_TurnsOnAndOff)
{
    BitArray16 bitflag1;

    bitflag1.SetBit(0, true);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.SetBit(0, false);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray16, ToggleBit_FlipsBitState)
{
    BitArray16 bitflag1;

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray16, GetNumberBitsSetOn_CountsCorrectly)
{
    BitArray16 bitflag1(1);
    EXPECT_EQ(bitflag1.GetNumberBitsSetOn(), 1);

    BitArray16 bitflag2(0);
    EXPECT_EQ(bitflag2.GetNumberBitsSetOn(), 0);

    BitArray16 bitflag3(0xFFFF);
    EXPECT_EQ(bitflag3.GetNumberBitsSetOn(), 16);
}

TEST(BitArray16, BitwiseOr_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(2);

    unsigned char bit3 = bit1 | bit2;
    BitArray16 bitflag3 = bitflag1 | bitflag2;

    EXPECT_EQ(bit3, bitflag3.GetAllBits());
}

TEST(BitArray16, BitwiseAnd_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(2);

    unsigned char bit4 = bit1 & bit2;
    BitArray16 bitflag4 = bitflag1 & bitflag2;

    EXPECT_EQ(bit4, bitflag4.GetAllBits());
}

TEST(BitArray16, BitwiseXor_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(2);

    unsigned char bit5 = bit1 ^ bit2;
    BitArray16 bitflag5 = bitflag1 ^ bitflag2;

    EXPECT_EQ(bit5, bitflag5.GetAllBits());
}

TEST(BitArray16, CompoundOrAssignment_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(2);

    bit1 |= bit2;
    bitflag1 |= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray16, CompoundAndAssignment_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(2);

    bit1 &= bit2;
    bitflag1 &= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray16, CompoundXorAssignment_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray16 bitflag1(1);
    BitArray16 bitflag2(2);

    bit1 ^= bit2;
    bitflag1 ^= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray16, GetByte_ReturnsCorrectBytes)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray16 bitflag1(1);

    BitArray8 bitarray2(bitflag1.GetByte(0));
    BitArray8 bitarray3(bitflag1.GetByte(1));

    EXPECT_TRUE(bitarray2[0]);
    EXPECT_FALSE(bitarray2[1]);
    EXPECT_TRUE(bitarray3.IsAllOff());
}

// ============================================================================
// BitArray32 Tests
// ============================================================================

TEST(BitArray32, DefaultConstructor_IsAllOff)
{
    BitArray32 bitflag;

    EXPECT_EQ(sizeof(bitflag), 4);
    EXPECT_TRUE(bitflag.IsAllOff());
    EXPECT_FALSE(bitflag.IsAllOn());
}

TEST(BitArray32, ConstructWithZero_AllBitsOff)
{
    BitArray32 bitflag1(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray32::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray32, ConstructWithOne_FirstBitOn)
{
    BitArray32 bitflag2(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray32::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray32, ConstructWithAllBitsOn_IsAllOn)
{
    BitArray32 bitflag3(0xFFFFFFFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray32::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray32, ArrayAccessor_NegativeIndex_Asserts)
{
    BitArray32 bitflag3(0xFFFFFFFF);
    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
}

TEST(BitArray32, ArrayAccessor_OutOfBounds_Asserts)
{
    BitArray32 bitflag3(0xFFFFFFFF);
    EXPECT_DEATH({ bool a = bitflag3[32]; (void)a; }, "");
}

TEST(BitArray32, CopyConstructor_CopiesAllBits)
{
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(bitflag1);

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray32, AssignmentFromValue_SetsCorrectly)
{
    BitArray32 bitflag1 = 1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);
}

TEST(BitArray32, AssignmentOperator_CopiesAllBits)
{
    BitArray32 bitflag1(1);
    BitArray32 bitflag2 = bitflag1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray32, EqualityOperators_CompareCorrectly)
{
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(1);
    BitArray32 bitflag3(2);

    EXPECT_EQ(bitflag1, bitflag2);
    EXPECT_NE(bitflag1, bitflag3);
    EXPECT_NE(bitflag2, bitflag3);
}

TEST(BitArray32, Invert_FlipsAllBits)
{
    BitArray32 bitflag1;
    BitArray32 bitflag2(bitflag1);

    bitflag2.Invert();

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_TRUE(bitflag2.IsAllOn());
}

TEST(BitArray32, Clear_SetsAllBitsOff)
{
    BitArray32 bitflag1(2);

    EXPECT_FALSE(bitflag1.IsAllOff());

    bitflag1.Clear();

    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray32, SetAllBits_WithZero_AllBitsOff)
{
    BitArray32 bitflag1;
    bitflag1.SetAllBits(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray32::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray32, SetAllBits_WithOne_FirstBitOn)
{
    BitArray32 bitflag2;
    bitflag2.SetAllBits(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray32::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray32, SetAllBits_WithAllOnes_IsAllOn)
{
    BitArray32 bitflag3(0xFFFFFFFF);
    bitflag3.SetAllBits(0xFFFFFFFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray32::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray32, SetAllBits_OutOfBoundsAccess_Asserts)
{
    BitArray32 bitflag3(0xFFFFFFFF);
    bitflag3.SetAllBits(0xFFFFFFFF);

    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
    EXPECT_DEATH({ bool a = bitflag3[32]; (void)a; }, "");
}

TEST(BitArray32, SetBit_TurnsOnAndOff)
{
    BitArray32 bitflag1;

    bitflag1.SetBit(0, true);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.SetBit(0, false);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray32, ToggleBit_FlipsBitState)
{
    BitArray32 bitflag1;

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray32, GetNumberBitsSetOn_CountsCorrectly)
{
    BitArray32 bitflag1(1);
    EXPECT_EQ(bitflag1.GetNumberBitsSetOn(), 1);

    BitArray32 bitflag2(0);
    EXPECT_EQ(bitflag2.GetNumberBitsSetOn(), 0);

    BitArray32 bitflag3(0xFFFFFFFF);
    EXPECT_EQ(bitflag3.GetNumberBitsSetOn(), 32);
}

TEST(BitArray32, BitwiseOr_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(2);

    unsigned char bit3 = bit1 | bit2;
    BitArray32 bitflag3 = bitflag1 | bitflag2;

    EXPECT_EQ(bit3, bitflag3.GetAllBits());
}

TEST(BitArray32, BitwiseAnd_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(2);

    unsigned char bit4 = bit1 & bit2;
    BitArray32 bitflag4 = bitflag1 & bitflag2;

    EXPECT_EQ(bit4, bitflag4.GetAllBits());
}

TEST(BitArray32, BitwiseXor_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(2);

    unsigned char bit5 = bit1 ^ bit2;
    BitArray32 bitflag5 = bitflag1 ^ bitflag2;

    EXPECT_EQ(bit5, bitflag5.GetAllBits());
}

TEST(BitArray32, CompoundOrAssignment_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(2);

    bit1 |= bit2;
    bitflag1 |= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray32, CompoundAndAssignment_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(2);

    bit1 &= bit2;
    bitflag1 &= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray32, CompoundXorAssignment_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray32 bitflag1(1);
    BitArray32 bitflag2(2);

    bit1 ^= bit2;
    bitflag1 ^= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray32, GetByte_ReturnsCorrectBytes)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray32 bitflag1(1);

    BitArray8 bitarray2(bitflag1.GetByte(0));
    BitArray8 bitarray3(bitflag1.GetByte(1));

    EXPECT_TRUE(bitarray2[0]);
    EXPECT_FALSE(bitarray2[1]);
    EXPECT_TRUE(bitarray3.IsAllOff());
}

// ============================================================================
// BitArray64 Tests
// ============================================================================

TEST(BitArray64, DefaultConstructor_IsAllOff)
{
    BitArray64 bitflag;

    EXPECT_EQ(sizeof(bitflag), 8);
    EXPECT_TRUE(bitflag.IsAllOff());
    EXPECT_FALSE(bitflag.IsAllOn());
}

TEST(BitArray64, ConstructWithZero_AllBitsOff)
{
    BitArray64 bitflag1(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray64::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray64, ConstructWithOne_FirstBitOn)
{
    BitArray64 bitflag2(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray64::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray64, ConstructWithAllBitsOn_IsAllOn)
{
    BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray64::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray64, ArrayAccessor_NegativeIndex_Asserts)
{
    BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);
    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
}

TEST(BitArray64, ArrayAccessor_OutOfBounds_Asserts)
{
    BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);
    EXPECT_DEATH({ bool a = bitflag3[64]; (void)a; }, "");
}

TEST(BitArray64, CopyConstructor_CopiesAllBits)
{
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(bitflag1);

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray64, AssignmentFromValue_SetsCorrectly)
{
    BitArray64 bitflag1 = 1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);
}

TEST(BitArray64, AssignmentOperator_CopiesAllBits)
{
    BitArray64 bitflag1(1);
    BitArray64 bitflag2 = bitflag1;

    EXPECT_FALSE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    EXPECT_FALSE(bitflag2[1]);

    EXPECT_EQ(bitflag2, bitflag1);
}

TEST(BitArray64, EqualityOperators_CompareCorrectly)
{
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(1);
    BitArray64 bitflag3(2);

    EXPECT_EQ(bitflag1, bitflag2);
    EXPECT_NE(bitflag1, bitflag3);
    EXPECT_NE(bitflag2, bitflag3);
}

TEST(BitArray64, Invert_FlipsAllBits)
{
    BitArray64 bitflag1;
    BitArray64 bitflag2(bitflag1);

    bitflag2.Invert();

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_TRUE(bitflag2.IsAllOn());
}

TEST(BitArray64, Clear_SetsAllBitsOff)
{
    BitArray64 bitflag1(2);

    EXPECT_FALSE(bitflag1.IsAllOff());

    bitflag1.Clear();

    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray64, SetAllBits_WithZero_AllBitsOff)
{
    BitArray64 bitflag1;
    bitflag1.SetAllBits(0);

    EXPECT_TRUE(bitflag1.IsAllOff());
    EXPECT_FALSE(bitflag1.IsAllOn());

    for (unsigned int i = 0; i < BitArray64::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag1[i]);
        EXPECT_FALSE(bitflag1.GetBit(i));
    }
}

TEST(BitArray64, SetAllBits_WithOne_FirstBitOn)
{
    BitArray64 bitflag2;
    bitflag2.SetAllBits(1);

    EXPECT_FALSE(bitflag2.IsAllOff());
    EXPECT_FALSE(bitflag2.IsAllOn());
    EXPECT_TRUE(bitflag2[0]);
    for (unsigned int i = 1; i < BitArray64::kMaxNumberOfBits; i++)
    {
        EXPECT_FALSE(bitflag2[i]);
        EXPECT_FALSE(bitflag2.GetBit(i));
    }
}

TEST(BitArray64, SetAllBits_WithAllOnes_IsAllOn)
{
    BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);
    bitflag3.SetAllBits(0xFFFFFFFFFFFFFFFF);

    EXPECT_FALSE(bitflag3.IsAllOff());
    EXPECT_TRUE(bitflag3.IsAllOn());
    for (unsigned int i = 0; i < BitArray64::kMaxNumberOfBits; i++)
    {
        EXPECT_TRUE(bitflag3[i]);
        EXPECT_TRUE(bitflag3.GetBit(i));
    }
}

TEST(BitArray64, SetAllBits_OutOfBoundsAccess_Asserts)
{
    BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);
    bitflag3.SetAllBits(0xFFFFFFFFFFFFFFFF);

    EXPECT_DEATH({ bool a = bitflag3[-1]; (void)a; }, "");
    EXPECT_DEATH({ bool a = bitflag3[64]; (void)a; }, "");
}

TEST(BitArray64, SetBit_TurnsOnAndOff)
{
    BitArray64 bitflag1;

    bitflag1.SetBit(0, true);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.SetBit(0, false);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray64, ToggleBit_FlipsBitState)
{
    BitArray64 bitflag1;

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1[0]);
    EXPECT_FALSE(bitflag1[1]);

    bitflag1.ToggleBit(0);
    EXPECT_TRUE(bitflag1.IsAllOff());
}

TEST(BitArray64, GetNumberBitsSetOn_CountsCorrectly)
{
    BitArray64 bitflag1(1);
    EXPECT_EQ(bitflag1.GetNumberBitsSetOn(), 1);

    BitArray64 bitflag2(0);
    EXPECT_EQ(bitflag2.GetNumberBitsSetOn(), 0);

    BitArray64 bitflag3(0xFFFFFFFFFFFFFFFF);
    EXPECT_EQ(bitflag3.GetNumberBitsSetOn(), 64);
}

TEST(BitArray64, BitwiseOr_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(2);

    unsigned char bit3 = bit1 | bit2;
    BitArray64 bitflag3 = bitflag1 | bitflag2;

    EXPECT_EQ(bit3, bitflag3.GetAllBits());
}

TEST(BitArray64, BitwiseAnd_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(2);

    unsigned char bit4 = bit1 & bit2;
    BitArray64 bitflag4 = bitflag1 & bitflag2;

    EXPECT_EQ(bit4, bitflag4.GetAllBits());
}

TEST(BitArray64, BitwiseXor_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(2);

    unsigned char bit5 = bit1 ^ bit2;
    BitArray64 bitflag5 = bitflag1 ^ bitflag2;

    EXPECT_EQ(bit5, bitflag5.GetAllBits());
}

TEST(BitArray64, CompoundOrAssignment_CombinesBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(2);

    bit1 |= bit2;
    bitflag1 |= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray64, CompoundAndAssignment_MasksBits)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(2);

    bit1 &= bit2;
    bitflag1 &= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray64, CompoundXorAssignment_TogglesDifferences)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray64 bitflag1(1);
    BitArray64 bitflag2(2);

    bit1 ^= bit2;
    bitflag1 ^= bitflag2;

    EXPECT_EQ(bit1, bitflag1.GetAllBits());
}

TEST(BitArray64, GetByte_ReturnsCorrectBytes)
{
    unsigned char bit1 = 1;
    unsigned char bit2 = 2;
    BitArray64 bitflag1(1);

    BitArray8 bitarray2(bitflag1.GetByte(0));
    BitArray8 bitarray3(bitflag1.GetByte(1));

    EXPECT_TRUE(bitarray2[0]);
    EXPECT_FALSE(bitarray2[1]);
    EXPECT_TRUE(bitarray3.IsAllOff());
}
