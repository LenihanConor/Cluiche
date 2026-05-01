// TestCRC.cpp - Google Test unit tests for CRC
//
// Tests CRC and StringCRC from DiaCore CRC subsystem

#include <gtest/gtest.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <cstring>

using namespace Dia::Core;

// ==============================================================================
// CRC Basic Tests
// ==============================================================================

TEST(CRC, DefaultConstruction_ValueIsZero)
{
    CRC crc;

    EXPECT_EQ(crc.Value(), 0u);
}

TEST(CRC, ConstructWithValue_StoresValue)
{
    CRC crc(2);

    EXPECT_EQ(crc.Value(), 2u);
}

TEST(CRC, CopyConstructor_CopiesValue)
{
    CRC crc1(2);
    CRC crc2(crc1);

    EXPECT_EQ(crc2.Value(), 2u);
    EXPECT_EQ(crc1, crc2);
}

TEST(CRC, ConstructFromString_GeneratesSameCRCForSameString)
{
    CRC crc1("Conor's Test");
    CRC crc2("Conor's Test");

    EXPECT_EQ(crc1, crc2);
}

TEST(CRC, ConstructFromString_GeneratesDifferentCRCForDifferentString)
{
    CRC crc1("Test1");
    CRC crc2("Test2");

    EXPECT_NE(crc1, crc2);
}

// ==============================================================================
// CRC Assignment Tests
// ==============================================================================

TEST(CRC, AssignmentOperator_CopiesValue)
{
    CRC crc1(2);
    CRC crc2;

    crc2 = crc1;

    EXPECT_EQ(crc2, crc1);
}

TEST(CRC, AssignmentFromUnsignedInt_StoresValue)
{
    CRC crc;

    crc = 2u;

    EXPECT_EQ(crc.Value(), 2u);
}

TEST(CRC, ConversionToUnsignedInt_ReturnsValue)
{
    CRC crc(2);

    unsigned int value = crc;

    EXPECT_EQ(value, 2u);
}

// ==============================================================================
// CRC Comparison Tests
// ==============================================================================

TEST(CRC, OperatorEquals_SameValue_ReturnsTrue)
{
    CRC crc1(5);
    CRC crc2(5);

    EXPECT_EQ(crc1, crc2);
}

TEST(CRC, OperatorNotEquals_DifferentValue_ReturnsTrue)
{
    CRC crc1(5);
    CRC crc2(10);

    EXPECT_NE(crc1, crc2);
}

// ==============================================================================
// StringCRC Basic Tests
// ==============================================================================

TEST(StringCRC, DefaultConstruction_ValueIsZero)
{
    StringCRC crc;

    EXPECT_EQ(crc.Value(), 0u);
    EXPECT_EQ(strlen(crc.AsChar()), 0u);
}

TEST(StringCRC, ConstructFromString_StoresString)
{
    StringCRC crc("Conor");

    EXPECT_STREQ(crc.AsChar(), "Conor");
}

TEST(StringCRC, CopyConstructor_CopiesCRCAndString)
{
    StringCRC crc1("Conor");
    StringCRC crc2(crc1);

    EXPECT_EQ(crc1, crc2);
    EXPECT_STREQ(crc1.AsChar(), "Conor");
    EXPECT_STREQ(crc2.AsChar(), "Conor");
}

TEST(StringCRC, AssignmentOperator_CopiesCRCAndString)
{
    StringCRC crc1("Conor");
    StringCRC crc2;

    crc2 = crc1;

    EXPECT_EQ(crc1, crc2);
    EXPECT_STREQ(crc1.AsChar(), "Conor");
    EXPECT_STREQ(crc2.AsChar(), "Conor");
}

TEST(StringCRC, SameString_GeneratesSameCRC)
{
    StringCRC crc1("TestString");
    StringCRC crc2("TestString");

    EXPECT_EQ(crc1, crc2);
    EXPECT_STREQ(crc1.AsChar(), crc2.AsChar());
}

TEST(StringCRC, DifferentStrings_GenerateDifferentCRCs)
{
    StringCRC crc1("String1");
    StringCRC crc2("String2");

    EXPECT_NE(crc1, crc2);
}

// ==============================================================================
// StringCRC Comparison Tests
// ==============================================================================

TEST(StringCRC, OperatorEquals_SameString_ReturnsTrue)
{
    StringCRC crc1("Test");
    StringCRC crc2("Test");

    EXPECT_EQ(crc1, crc2);
}

TEST(StringCRC, OperatorNotEquals_DifferentString_ReturnsTrue)
{
    StringCRC crc1("Test1");
    StringCRC crc2("Test2");

    EXPECT_NE(crc1, crc2);
}

// ==============================================================================
// StringCRC Empty String Tests
// ==============================================================================

TEST(StringCRC, EmptyString_ValueIsZero)
{
    StringCRC crc("");

    EXPECT_EQ(crc.Value(), 0u);
    EXPECT_EQ(strlen(crc.AsChar()), 0u);
}

TEST(StringCRC, EmptyStringEquals_AnotherEmptyString)
{
    StringCRC crc1("");
    StringCRC crc2("");

    EXPECT_EQ(crc1, crc2);
}

// ==============================================================================
// StringCRC Debug Mode Tests (conditional)
// ==============================================================================

#ifdef DEBUG
TEST(StringCRC, DebugMode_StoresStringData)
{
    StringCRC crc("DebugString");

    // In debug mode, string should be preserved
    EXPECT_STREQ(crc.AsChar(), "DebugString");
    EXPECT_GT(strlen(crc.AsChar()), 0u);
}
#else
TEST(StringCRC, ReleaseMode_MayStripStringData)
{
    StringCRC crc("ReleaseString");

    // In release mode, string may be stripped (implementation dependent)
    // Just verify CRC value exists
    EXPECT_GT(crc.Value(), 0u);
}
#endif

// ==============================================================================
// StringCRC Long String Tests
// ==============================================================================

TEST(StringCRC, LongString_GeneratesCRC)
{
    // StringCRC has 64 char limit - use string within that limit
    const char* longString = "This is a string to test CRC generation with longer input";
    StringCRC crc(longString);

    EXPECT_GT(crc.Value(), 0u);
    EXPECT_STREQ(crc.AsChar(), longString);
}

TEST(StringCRC, LongStringsEqual_GenerateSameCRC)
{
    // Must be under 64 chars (StringCRC::kStringLength)
    const char* str = "Another string for testing CRC collision detection here";
    StringCRC crc1(str);
    StringCRC crc2(str);

    EXPECT_EQ(crc1, crc2);
}

// ==============================================================================
// CRC Edge Cases
// ==============================================================================

TEST(CRC, MaxUnsignedIntValue_HandledCorrectly)
{
    CRC crc(0xFFFFFFFF);

    EXPECT_EQ(crc.Value(), 0xFFFFFFFFu);
}

TEST(CRC, ZeroValue_EqualsDefaultConstruction)
{
    CRC crc1(0u);
    CRC crc2;

    EXPECT_EQ(crc1, crc2);
}

// ==============================================================================
// StringCRC Special Characters
// ==============================================================================

TEST(StringCRC, SpecialCharacters_GeneratesCRC)
{
    StringCRC crc("Test!@#$%^&*()_+-=[]{}|;':\",./<>?");

    EXPECT_GT(crc.Value(), 0u);
}

TEST(StringCRC, WhitespaceString_GeneratesCRC)
{
    StringCRC crc("   ");

    EXPECT_GT(crc.Value(), 0u);
    EXPECT_EQ(strlen(crc.AsChar()), 3u);
}
