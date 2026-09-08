// TestMeshPassLighting.cpp
//
// Tests for the RGBA->float4 unpacking used in MeshRenderer::DrawCommand to
// convert MaterialDescriptor::baseColourRGBA (0xRRGGBBAA packed uint32) into
// the float[4] passed to u_baseColour.
//
// The unpacking is:
//   r = ((rgba >> 24) & 0xFF) / 255.0f
//   g = ((rgba >> 16) & 0xFF) / 255.0f
//   b = ((rgba >>  8) & 0xFF) / 255.0f
//   a = ((rgba      ) & 0xFF) / 255.0f
//
// These tests are pure arithmetic — no bgfx context required.

#include <gtest/gtest.h>
#include <stdint.h>

namespace
{
    // Mirrors the unpacking in MeshRenderer::DrawCommand exactly.
    void UnpackRGBA(uint32_t rgba, float out[4])
    {
        out[0] = static_cast<float>((rgba >> 24) & 0xFF) / 255.0f;
        out[1] = static_cast<float>((rgba >> 16) & 0xFF) / 255.0f;
        out[2] = static_cast<float>((rgba >>  8) & 0xFF) / 255.0f;
        out[3] = static_cast<float>((rgba      ) & 0xFF) / 255.0f;
    }
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_OpaqueWhite)
{
    float c[4];
    UnpackRGBA(0xFFFFFFFF, c);

    EXPECT_FLOAT_EQ(c[0], 1.0f);
    EXPECT_FLOAT_EQ(c[1], 1.0f);
    EXPECT_FLOAT_EQ(c[2], 1.0f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_OpaqueBlack)
{
    float c[4];
    UnpackRGBA(0x000000FF, c);

    EXPECT_FLOAT_EQ(c[0], 0.0f);
    EXPECT_FLOAT_EQ(c[1], 0.0f);
    EXPECT_FLOAT_EQ(c[2], 0.0f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_PureRed)
{
    float c[4];
    UnpackRGBA(0xFF0000FF, c);

    EXPECT_FLOAT_EQ(c[0], 1.0f);
    EXPECT_FLOAT_EQ(c[1], 0.0f);
    EXPECT_FLOAT_EQ(c[2], 0.0f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_PureGreen)
{
    float c[4];
    UnpackRGBA(0x00FF00FF, c);

    EXPECT_FLOAT_EQ(c[0], 0.0f);
    EXPECT_FLOAT_EQ(c[1], 1.0f);
    EXPECT_FLOAT_EQ(c[2], 0.0f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_PureBlue)
{
    float c[4];
    UnpackRGBA(0x0000FFFF, c);

    EXPECT_FLOAT_EQ(c[0], 0.0f);
    EXPECT_FLOAT_EQ(c[1], 0.0f);
    EXPECT_FLOAT_EQ(c[2], 1.0f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_HalfAlpha)
{
    // 0x80 = 128 -> 128/255 ≈ 0.5020
    float c[4];
    UnpackRGBA(0xFFFFFF80, c);

    EXPECT_FLOAT_EQ(c[0], 1.0f);
    EXPECT_FLOAT_EQ(c[1], 1.0f);
    EXPECT_FLOAT_EQ(c[2], 1.0f);
    EXPECT_NEAR(c[3], 128.0f / 255.0f, 1e-6f);
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_DefaultMaterial)
{
    // default_3d material: 0xCCCCCCFF — grey, fully opaque.
    float c[4];
    UnpackRGBA(0xCCCCCCFF, c);

    const float expected = static_cast<float>(0xCC) / 255.0f;
    EXPECT_NEAR(c[0], expected, 1e-6f);
    EXPECT_NEAR(c[1], expected, 1e-6f);
    EXPECT_NEAR(c[2], expected, 1e-6f);
    EXPECT_FLOAT_EQ(c[3], 1.0f);
}

TEST(DiaBgfx3D_MeshPassLightingTest, UnpackRGBA_ChannelsAreIndependent)
{
    // Each channel independent: R=0x10, G=0x20, B=0x30, A=0x40
    float c[4];
    UnpackRGBA(0x10203040, c);

    EXPECT_NEAR(c[0], 0x10 / 255.0f, 1e-6f);
    EXPECT_NEAR(c[1], 0x20 / 255.0f, 1e-6f);
    EXPECT_NEAR(c[2], 0x30 / 255.0f, 1e-6f);
    EXPECT_NEAR(c[3], 0x40 / 255.0f, 1e-6f);
}
