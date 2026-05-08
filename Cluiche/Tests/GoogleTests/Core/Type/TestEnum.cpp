// TestEnum.cpp - Google Test unit tests for EnumClass
//
// Tests CLASSEDENUM macro system from DiaCore

#include <gtest/gtest.h>
#include <DiaCore/Core/EnumClass.h>
#include <DiaCore/Strings/String32.h>

using namespace Dia::Core;

// ==============================================================================
// Test Enum Definition
// ==============================================================================

CLASSEDENUM(EStyle,
    CE_ITEMVAL(None, -1)
    CE_ITEM(Attacking)
    CE_ITEM(Defending)
    , None
);

// ==============================================================================
// Enum Construction and Comparison Tests
// ==============================================================================

TEST(Enum, DefaultConstruction_InitializesToDefault)
{
    EStyle style;

    EXPECT_EQ(style, EStyle::None);
}

TEST(Enum, InitializeToNone_EqualsNone)
{
    EStyle style = EStyle::None;

    EXPECT_EQ(style, EStyle::None);
}

TEST(Enum, InitializeToDefending_EqualsDefending)
{
    EStyle style = EStyle::Defending;

    EXPECT_EQ(style, EStyle::Defending);
}

TEST(Enum, InitializeToAttacking_EqualsAttacking)
{
    EStyle style = EStyle::Attacking;

    EXPECT_EQ(style, EStyle::Attacking);
}

// ==============================================================================
// Enum Conversion to Int Tests
// ==============================================================================

TEST(Enum, ConvertToInt_None_ReturnsNegativeOne)
{
    EStyle style = EStyle::None;

    int value = style;

    EXPECT_EQ(value, -1);
}

TEST(Enum, ConvertToInt_Attacking_ReturnsZero)
{
    EStyle style = EStyle::Attacking;

    int value = style;

    EXPECT_EQ(value, 0);
}

TEST(Enum, ConvertToInt_Defending_ReturnsOne)
{
    EStyle style = EStyle::Defending;

    int value = style;

    EXPECT_EQ(value, 1);
}

// ==============================================================================
// AsString Tests
// ==============================================================================

TEST(Enum, AsString_None_ReturnsNone)
{
    EStyle style = EStyle::None;

    Containers::String32 name = style.AsString();

    EXPECT_STREQ(name.AsCStr(), "None");
}

TEST(Enum, AsString_Attacking_ReturnsAttacking)
{
    EStyle style = EStyle::Attacking;

    Containers::String32 name = style.AsString();

    EXPECT_STREQ(name.AsCStr(), "Attacking");
}

TEST(Enum, AsString_Defending_ReturnsDefending)
{
    EStyle style = EStyle::Defending;

    Containers::String32 name = style.AsString();

    EXPECT_STREQ(name.AsCStr(), "Defending");
}

// ==============================================================================
// AsStringEnumClass Tests
// ==============================================================================

TEST(Enum, AsStringEnumClass_ReturnsClassName)
{
    EStyle style1 = EStyle::None;
    EStyle style2 = EStyle::Attacking;
    EStyle style3 = EStyle::Defending;

    Containers::String32 className1 = style1.AsStringEnumClass();
    Containers::String32 className2 = style2.AsStringEnumClass();
    Containers::String32 className3 = style3.AsStringEnumClass();

    EXPECT_STREQ(className1.AsCStr(), "EStyle");
    EXPECT_STREQ(className2.AsCStr(), "EStyle");
    EXPECT_STREQ(className3.AsCStr(), "EStyle");
}

// ==============================================================================
// AsStringWithEnumName Tests
// ==============================================================================

TEST(Enum, AsStringWithEnumName_None_ReturnsQualifiedName)
{
    EStyle style = EStyle::None;
    Containers::String32 fullName;

    style.AsStringWithEnumName(fullName.AsCStr(), fullName.Size());

    EXPECT_STREQ(fullName.AsCStr(), "EStyle::None");
}

TEST(Enum, AsStringWithEnumName_Attacking_ReturnsQualifiedName)
{
    EStyle style = EStyle::Attacking;
    Containers::String32 fullName;

    style.AsStringWithEnumName(fullName.AsCStr(), fullName.Size());

    EXPECT_STREQ(fullName.AsCStr(), "EStyle::Attacking");
}

TEST(Enum, AsStringWithEnumName_Defending_ReturnsQualifiedName)
{
    EStyle style = EStyle::Defending;
    Containers::String32 fullName;

    style.AsStringWithEnumName(fullName.AsCStr(), fullName.Size());

    EXPECT_STREQ(fullName.AsCStr(), "EStyle::Defending");
}

// ==============================================================================
// Enum Value Assignment Tests
// ==============================================================================

TEST(Enum, CustomValue_None_HasNegativeOne)
{
    // CE_ITEMVAL(None, -1) assigns custom value
    EStyle style = EStyle::None;

    EXPECT_EQ(static_cast<int>(style), -1);
}

TEST(Enum, AutoIncrementValue_Attacking_HasZero)
{
    // First CE_ITEM after -1 should be 0
    EStyle style = EStyle::Attacking;

    EXPECT_EQ(static_cast<int>(style), 0);
}

TEST(Enum, AutoIncrementValue_Defending_HasOne)
{
    // Second CE_ITEM should be 1
    EStyle style = EStyle::Defending;

    EXPECT_EQ(static_cast<int>(style), 1);
}
