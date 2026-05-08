// TestPrimitiveType.cpp - Google Test unit tests for PrimitiveType
//
// Tests PrimitiveType enum from DiaGraphics

#include <gtest/gtest.h>
#include <DiaGraphics/Misc/PrimitiveType.h>

using namespace Dia::Graphics;

// ==============================================================================
// PrimitiveType Enum Value Tests
// ==============================================================================

TEST(PrimitiveType, Points_Exists)
{
    PrimitiveType type = PrimitiveType::Points;
    EXPECT_EQ(type, PrimitiveType::Points);
}

TEST(PrimitiveType, Lines_Exists)
{
    PrimitiveType type = PrimitiveType::Lines;
    EXPECT_EQ(type, PrimitiveType::Lines);
}

TEST(PrimitiveType, LineStrip_Exists)
{
    PrimitiveType type = PrimitiveType::LineStrip;
    EXPECT_EQ(type, PrimitiveType::LineStrip);
}

TEST(PrimitiveType, Triangles_Exists)
{
    PrimitiveType type = PrimitiveType::Triangles;
    EXPECT_EQ(type, PrimitiveType::Triangles);
}

TEST(PrimitiveType, TriangleStrip_Exists)
{
    PrimitiveType type = PrimitiveType::TriangleStrip;
    EXPECT_EQ(type, PrimitiveType::TriangleStrip);
}

TEST(PrimitiveType, TriangleFan_Exists)
{
    PrimitiveType type = PrimitiveType::TriangleFan;
    EXPECT_EQ(type, PrimitiveType::TriangleFan);
}

// ==============================================================================
// PrimitiveType Distinction Tests
// ==============================================================================

TEST(PrimitiveType, AllValuesAreDifferent)
{
    EXPECT_NE(PrimitiveType::Points, PrimitiveType::Lines);
    EXPECT_NE(PrimitiveType::Lines, PrimitiveType::LineStrip);
    EXPECT_NE(PrimitiveType::LineStrip, PrimitiveType::Triangles);
    EXPECT_NE(PrimitiveType::Triangles, PrimitiveType::TriangleStrip);
    EXPECT_NE(PrimitiveType::TriangleStrip, PrimitiveType::TriangleFan);
}

TEST(PrimitiveType, LineTypesAreDifferent)
{
    EXPECT_NE(PrimitiveType::Lines, PrimitiveType::LineStrip);
}

TEST(PrimitiveType, TriangleTypesAreDifferent)
{
    EXPECT_NE(PrimitiveType::Triangles, PrimitiveType::TriangleStrip);
    EXPECT_NE(PrimitiveType::TriangleStrip, PrimitiveType::TriangleFan);
    EXPECT_NE(PrimitiveType::Triangles, PrimitiveType::TriangleFan);
}

// ==============================================================================
// PrimitiveType Assignment Tests
// ==============================================================================

TEST(PrimitiveType, CanAssignAndCompare)
{
    PrimitiveType type1 = PrimitiveType::Points;
    PrimitiveType type2 = type1;

    EXPECT_EQ(type1, type2);
}

TEST(PrimitiveType, CanChangeValue)
{
    PrimitiveType type = PrimitiveType::Points;
    EXPECT_EQ(type, PrimitiveType::Points);

    type = PrimitiveType::Triangles;
    EXPECT_EQ(type, PrimitiveType::Triangles);
    EXPECT_NE(type, PrimitiveType::Points);
}

// ==============================================================================
// PrimitiveType Switch Statement Tests
// ==============================================================================

TEST(PrimitiveType, CanUseInSwitchStatement)
{
    PrimitiveType type = PrimitiveType::Triangles;
    int result = 0;

    switch (type)
    {
    case PrimitiveType::Points:
        result = 1;
        break;
    case PrimitiveType::Lines:
        result = 2;
        break;
    case PrimitiveType::LineStrip:
        result = 3;
        break;
    case PrimitiveType::Triangles:
        result = 4;
        break;
    case PrimitiveType::TriangleStrip:
        result = 5;
        break;
    case PrimitiveType::TriangleFan:
        result = 6;
        break;
    }

    EXPECT_EQ(result, 4);
}
