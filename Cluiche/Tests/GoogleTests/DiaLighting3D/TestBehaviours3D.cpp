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
