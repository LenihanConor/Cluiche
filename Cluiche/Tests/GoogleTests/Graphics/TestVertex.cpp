// TestVertex.cpp - Google Test unit tests for Vertex
//
// Tests Vertex structure from DiaGraphics

#include <gtest/gtest.h>
#include <DiaGraphics/Misc/Vertex.h>
#include <DiaGraphics/Misc/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// ==============================================================================
// Vertex Construction Tests
// ==============================================================================

TEST(Vertex, DefaultConstructor_InitializesWithDefaults)
{
    Vertex v;

    EXPECT_EQ(v.position.x, 0.0f);
    EXPECT_EQ(v.position.y, 0.0f);
    EXPECT_EQ(v.color, RGBA::White);
    EXPECT_EQ(v.texCoords.x, 0.0f);
    EXPECT_EQ(v.texCoords.y, 0.0f);
}

TEST(Vertex, PositionConstructor_SetsPositionWhiteColor)
{
    Vector2D pos(10.0f, 20.0f);
    Vertex v(pos);

    EXPECT_EQ(v.position.x, 10.0f);
    EXPECT_EQ(v.position.y, 20.0f);
    EXPECT_EQ(v.color, RGBA::White);
    EXPECT_EQ(v.texCoords.x, 0.0f);
    EXPECT_EQ(v.texCoords.y, 0.0f);
}

TEST(Vertex, PositionColorConstructor_SetsPositionAndColor)
{
    Vector2D pos(15.0f, 25.0f);
    RGBA color = RGBA::Red;
    Vertex v(pos, color);

    EXPECT_EQ(v.position.x, 15.0f);
    EXPECT_EQ(v.position.y, 25.0f);
    EXPECT_EQ(v.color, RGBA::Red);
    EXPECT_EQ(v.texCoords.x, 0.0f);
    EXPECT_EQ(v.texCoords.y, 0.0f);
}

TEST(Vertex, PositionTexCoordsConstructor_SetsPositionAndTexCoords)
{
    Vector2D pos(30.0f, 40.0f);
    Vector2D tex(0.5f, 0.75f);
    Vertex v(pos, tex);

    EXPECT_EQ(v.position.x, 30.0f);
    EXPECT_EQ(v.position.y, 40.0f);
    EXPECT_EQ(v.color, RGBA::White);
    EXPECT_EQ(v.texCoords.x, 0.5f);
    EXPECT_EQ(v.texCoords.y, 0.75f);
}

TEST(Vertex, FullConstructor_SetsAllFields)
{
    Vector2D pos(100.0f, 200.0f);
    RGBA color = RGBA::Blue;
    Vector2D tex(1.0f, 0.0f);
    Vertex v(pos, color, tex);

    EXPECT_EQ(v.position.x, 100.0f);
    EXPECT_EQ(v.position.y, 200.0f);
    EXPECT_EQ(v.color, RGBA::Blue);
    EXPECT_EQ(v.texCoords.x, 1.0f);
    EXPECT_EQ(v.texCoords.y, 0.0f);
}

// ==============================================================================
// Vertex Memory Layout Tests (Critical for SFML compatibility)
// ==============================================================================

TEST(Vertex, MemoryLayout_PositionFirst)
{
    Vertex v;
    void* vertexAddr = &v;
    void* positionAddr = &v.position;

    EXPECT_EQ(vertexAddr, positionAddr);
}

TEST(Vertex, MemoryLayout_ColorAfterPosition)
{
    Vertex v;
    size_t positionOffset = offsetof(Vertex, position);
    size_t colorOffset = offsetof(Vertex, color);

    EXPECT_EQ(positionOffset, 0);
    EXPECT_GT(colorOffset, positionOffset);
}

TEST(Vertex, MemoryLayout_TexCoordsLast)
{
    Vertex v;
    size_t colorOffset = offsetof(Vertex, color);
    size_t texCoordsOffset = offsetof(Vertex, texCoords);

    EXPECT_GT(texCoordsOffset, colorOffset);
}
