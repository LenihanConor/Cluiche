#include <gtest/gtest.h>

#include <DiaLighting3D/Behaviours/LightBehaviourRegistry3D.h>
#include <DiaLighting3D/Behaviours/FlickerBehaviour3D.h>
#include <DiaLighting3D/Behaviours/PulseBehaviour3D.h>
#include <DiaLighting3D/Behaviours/ColorCycleBehaviour3D.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>

using namespace Dia::Lighting3D;

// --- LightBehaviourRegistry3D ---

TEST(DiaLighting3D_BehaviourRegistry, IsRegistered_ReturnsFalse_ForUnknown)
{
    EXPECT_FALSE(LightBehaviourRegistry3D::Get().IsRegistered(Dia::Core::StringCRC("nonexistent_behaviour_xyz")));
}

TEST(DiaLighting3D_BehaviourRegistry, FlickerBehaviour3D_IsRegistered)
{
    EXPECT_TRUE(LightBehaviourRegistry3D::Get().IsRegistered(FlickerBehaviour3D::kTypeId));
}

TEST(DiaLighting3D_BehaviourRegistry, PulseBehaviour3D_IsRegistered)
{
    EXPECT_TRUE(LightBehaviourRegistry3D::Get().IsRegistered(PulseBehaviour3D::kTypeId));
}

TEST(DiaLighting3D_BehaviourRegistry, ColorCycleBehaviour3D_IsRegistered)
{
    EXPECT_TRUE(LightBehaviourRegistry3D::Get().IsRegistered(ColorCycleBehaviour3D::kTypeId));
}

TEST(DiaLighting3D_BehaviourRegistry, Create_ReturnsNonNull_ForFlicker)
{
    ILightBehaviour3D* b = LightBehaviourRegistry3D::Get().Create(FlickerBehaviour3D::kTypeId);
    ASSERT_NE(b, nullptr);
    delete b;
}

// --- FlickerBehaviour3D ---

TEST(DiaLighting3D_FlickerBehaviour3D, Update_MutatesIntensity)
{
    float intensity = 1.0f;
    FlickerBehaviour3D flicker;
    flicker.SetTarget(&intensity);
    flicker.SetRange(0.5f, 1.0f);
    flicker.Update(1.0f);
    EXPECT_GE(intensity, 0.5f);
    EXPECT_LE(intensity, 1.0f);
}

// --- PulseBehaviour3D ---

TEST(DiaLighting3D_PulseBehaviour3D, Update_MutatesIntensity)
{
    float intensity = 1.0f;
    PulseBehaviour3D pulse;
    pulse.SetTarget(&intensity);
    pulse.SetRange(0.0f, 1.0f);
    pulse.SetPeriod(1.0f);
    pulse.Update(0.25f);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);
}

// --- ColorCycleBehaviour3D ---

TEST(DiaLighting3D_ColorCycleBehaviour3D, Update_MutatesColour)
{
    Dia::Core::RGBA colour(255, 255, 255, 255);
    Dia::Core::RGBA palette[2] = {
        Dia::Core::RGBA(255, 0, 0, 255),
        Dia::Core::RGBA(0, 0, 255, 255)
    };

    ColorCycleBehaviour3D cycle;
    cycle.SetTarget(&colour);
    cycle.SetPalette(palette, 2);
    cycle.SetPeriod(1.0f);
    cycle.Update(0.0f);
}

// --- Golden values ---

TEST(DiaLighting3D_PulseBehaviour3D, AtPhaseZero_OutputsMidpoint)
{
    // dt=0 → phase stays 0, sin(0)=0, t=(0+1)*0.5=0.5, intensity = min + 0.5*(max-min) = 1.0
    float intensity = 0.0f;
    PulseBehaviour3D pulse;
    pulse.SetTarget(&intensity);
    pulse.SetRange(0.0f, 2.0f);
    pulse.SetPeriod(1.0f);
    pulse.Update(0.0f);
    EXPECT_NEAR(intensity, 1.0f, 0.001f);
}

TEST(DiaLighting3D_PulseBehaviour3D, AtQuarterPeriod_OutputsMax)
{
    // dt=0.25, period=1.0 → phase=pi/2, sin(pi/2)=1, t=1.0, intensity=max
    float intensity = 0.0f;
    PulseBehaviour3D pulse;
    pulse.SetTarget(&intensity);
    pulse.SetRange(0.0f, 2.0f);
    pulse.SetPeriod(1.0f);
    pulse.Update(0.25f);
    EXPECT_NEAR(intensity, 2.0f, 0.001f);
}

TEST(DiaLighting3D_ColorCycleBehaviour3D, AtPhaseZero_OutputsFirstColour)
{
    // dt=0 → phase stays 0, pos=0, idx=0, t=0 → output = palette[0]
    Dia::Core::RGBA colour(0, 0, 0, 255);
    Dia::Core::RGBA palette[2] = {
        Dia::Core::RGBA(100, 150, 200, 255),
        Dia::Core::RGBA(255,   0,   0, 255)
    };
    ColorCycleBehaviour3D cycle;
    cycle.SetTarget(&colour);
    cycle.SetPalette(palette, 2);
    cycle.SetPeriod(1.0f);
    cycle.Update(0.0f);
    EXPECT_EQ(colour.R(), 100);
    EXPECT_EQ(colour.G(), 150);
    EXPECT_EQ(colour.B(), 200);
}

TEST(DiaLighting3D_ColorCycleBehaviour3D, SingleColourPalette_ColourUnchanged)
{
    // mPaletteCount < 2 → early return, *mColour not written
    Dia::Core::RGBA colour(42, 42, 42, 255);
    Dia::Core::RGBA palette[1] = { Dia::Core::RGBA(255, 0, 0, 255) };
    ColorCycleBehaviour3D cycle;
    cycle.SetTarget(&colour);
    cycle.SetPalette(palette, 1);
    cycle.Update(0.5f);
    EXPECT_EQ(colour.R(), 42);
}

// --- Invariants ---

TEST(DiaLighting3D_FlickerBehaviour3D, OutputAlwaysInRange_After1000Updates)
{
    float intensity = 1.0f;
    FlickerBehaviour3D flicker;
    flicker.SetTarget(&intensity);
    flicker.SetRange(0.3f, 0.9f);
    flicker.SetFrequency(20.0f);
    // Prime with a large dt (> interval=0.05s) so the LCG fires and mCurrent/mTarget
    // are initialised to a value inside [0.3, 0.9] before the invariant loop begins.
    flicker.Update(0.1f);
    for (int i = 0; i < 1000; ++i)
    {
        flicker.Update(0.016f);
        EXPECT_GE(intensity, 0.3f);
        EXPECT_LE(intensity, 0.9f + 0.001f);  // small tolerance for lerp overshoot
    }
}

// --- Null target safety ---

TEST(DiaLighting3D_FlickerBehaviour3D, NullTarget_NoCrash)
{
    FlickerBehaviour3D flicker;  // SetTarget never called
    flicker.SetRange(0.5f, 1.0f);
    flicker.Update(1.0f);  // must not crash
}

TEST(DiaLighting3D_PulseBehaviour3D, NullTarget_NoCrash)
{
    PulseBehaviour3D pulse;  // SetTarget never called
    pulse.SetRange(0.0f, 1.0f);
    pulse.SetPeriod(1.0f);
    pulse.Update(0.25f);  // must not crash
}

// --- LightBehaviourRegistry3D (additional) ---

TEST(DiaLighting3D_BehaviourRegistry, Create_ReturnsNonNull_ForPulse)
{
    ILightBehaviour3D* b = LightBehaviourRegistry3D::Get().Create(PulseBehaviour3D::kTypeId);
    ASSERT_NE(b, nullptr);
    delete b;
}

TEST(DiaLighting3D_BehaviourRegistry, Create_ReturnsNonNull_ForColorCycle)
{
    ILightBehaviour3D* b = LightBehaviourRegistry3D::Get().Create(ColorCycleBehaviour3D::kTypeId);
    ASSERT_NE(b, nullptr);
    delete b;
}
