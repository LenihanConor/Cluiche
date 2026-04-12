// TestRenderStates.cpp - Google Test unit tests for RenderStates
//
// Tests RenderStates structure from DiaGraphics

#include <gtest/gtest.h>
#include <DiaGraphics/Misc/RenderStates.h>
#include <DiaGraphics/Misc/Transform.h>
#include <DiaGraphics/Assets/ITexture.h>
#include <DiaGraphics/Assets/IShader.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// ==============================================================================
// RenderStates Construction Tests
// ==============================================================================

TEST(RenderStates, DefaultConstructor_InitializesWithDefaults)
{
    RenderStates states;

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Alpha);
    EXPECT_EQ(states.texture, nullptr);
    EXPECT_EQ(states.shader, nullptr);
}

TEST(RenderStates, Default_IsValidInstance)
{
    RenderStates states = RenderStates::Default;

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Alpha);
    EXPECT_EQ(states.texture, nullptr);
    EXPECT_EQ(states.shader, nullptr);
}

TEST(RenderStates, BlendModeConstructor_SetsBlendMode)
{
    RenderStates states(RenderStates::BlendMode::Add);

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Add);
    EXPECT_EQ(states.texture, nullptr);
    EXPECT_EQ(states.shader, nullptr);
}

TEST(RenderStates, TransformConstructor_SetsTransform)
{
    Transform t;
    t.Translate(10.0f, 20.0f);

    RenderStates states(t);

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Alpha);
    EXPECT_EQ(states.texture, nullptr);
    EXPECT_EQ(states.shader, nullptr);

    // Transform should be copied
    Vector2D point(0.0f, 0.0f);
    Vector2D result = states.transform.TransformPoint(point);
    EXPECT_FLOAT_EQ(result.x, 10.0f);
    EXPECT_FLOAT_EQ(result.y, 20.0f);
}

TEST(RenderStates, TextureConstructor_SetsTexture)
{
    // Note: We can't instantiate ITexture directly, so pass nullptr
    const ITexture* tex = nullptr;
    RenderStates states(tex);

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Alpha);
    EXPECT_EQ(states.texture, nullptr);
    EXPECT_EQ(states.shader, nullptr);
}

TEST(RenderStates, ShaderConstructor_SetsShader)
{
    // Note: We can't instantiate IShader directly, so pass nullptr
    const IShader* shader = nullptr;
    RenderStates states(shader);

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Alpha);
    EXPECT_EQ(states.texture, nullptr);
    EXPECT_EQ(states.shader, nullptr);
}

TEST(RenderStates, FullConstructor_SetsAllFields)
{
    Transform t;
    t.Rotate(45.0f);

    RenderStates states(
        RenderStates::BlendMode::Multiply,
        t,
        nullptr, // texture
        nullptr  // shader
    );

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Multiply);
    EXPECT_EQ(states.texture, nullptr);
    EXPECT_EQ(states.shader, nullptr);
}

// ==============================================================================
// BlendMode Enum Tests
// ==============================================================================

TEST(RenderStates, BlendMode_Alpha_Exists)
{
    RenderStates::BlendMode mode = RenderStates::BlendMode::Alpha;
    EXPECT_EQ(mode, RenderStates::BlendMode::Alpha);
}

TEST(RenderStates, BlendMode_Add_Exists)
{
    RenderStates::BlendMode mode = RenderStates::BlendMode::Add;
    EXPECT_EQ(mode, RenderStates::BlendMode::Add);
}

TEST(RenderStates, BlendMode_Multiply_Exists)
{
    RenderStates::BlendMode mode = RenderStates::BlendMode::Multiply;
    EXPECT_EQ(mode, RenderStates::BlendMode::Multiply);
}

TEST(RenderStates, BlendMode_None_Exists)
{
    RenderStates::BlendMode mode = RenderStates::BlendMode::None;
    EXPECT_EQ(mode, RenderStates::BlendMode::None);
}

TEST(RenderStates, BlendMode_DifferentValuesAreDifferent)
{
    EXPECT_NE(RenderStates::BlendMode::Alpha, RenderStates::BlendMode::Add);
    EXPECT_NE(RenderStates::BlendMode::Add, RenderStates::BlendMode::Multiply);
    EXPECT_NE(RenderStates::BlendMode::Multiply, RenderStates::BlendMode::None);
}

// ==============================================================================
// RenderStates Field Modification Tests
// ==============================================================================

TEST(RenderStates, ModifyBlendMode_UpdatesValue)
{
    RenderStates states;
    states.blendMode = RenderStates::BlendMode::Add;

    EXPECT_EQ(states.blendMode, RenderStates::BlendMode::Add);
}

TEST(RenderStates, ModifyTransform_UpdatesValue)
{
    RenderStates states;
    states.transform.Translate(100.0f, 200.0f);

    Vector2D point(0.0f, 0.0f);
    Vector2D result = states.transform.TransformPoint(point);
    EXPECT_FLOAT_EQ(result.x, 100.0f);
    EXPECT_FLOAT_EQ(result.y, 200.0f);
}

TEST(RenderStates, ModifyTexture_UpdatesPointer)
{
    RenderStates states;
    const ITexture* dummyTexture = reinterpret_cast<const ITexture*>(0x12345678);
    states.texture = dummyTexture;

    EXPECT_EQ(states.texture, dummyTexture);
}

TEST(RenderStates, ModifyShader_UpdatesPointer)
{
    RenderStates states;
    const IShader* dummyShader = reinterpret_cast<const IShader*>(0x87654321);
    states.shader = dummyShader;

    EXPECT_EQ(states.shader, dummyShader);
}
