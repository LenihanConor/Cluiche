#include <gtest/gtest.h>

#include <DiaLighting2D/PointLight2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaLighting2D/Testing/LightBuilder.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using DynamicArrayLightPtrs = Dia::Core::Containers::DynamicArrayC<const Dia::Lighting2D::PointLight2D*, 32>;

using namespace Dia::Lighting2D;

// ---------------------------------------------------------------------------
// PointLight2D value type
// ---------------------------------------------------------------------------

TEST(DiaLighting2D_PointLight2D, DefaultConstruct_ExpectedDefaults)
{
    PointLight2D light;
    EXPECT_FLOAT_EQ(light.position.x, 0.0f);
    EXPECT_FLOAT_EQ(light.position.y, 0.0f);
    EXPECT_FLOAT_EQ(light.radius,    100.0f);
    EXPECT_FLOAT_EQ(light.intensity,   1.0f);
    EXPECT_EQ(light.layerMask, 0xFFFFFFFFu);
    EXPECT_TRUE(light.enabled);
}

TEST(DiaLighting2D_PointLight2D, CopySemantics_IndependentValues)
{
    PointLight2D a;
    a.position  = Dia::Maths::Vector2D(10.0f, 20.0f);
    a.radius    = 200.0f;
    PointLight2D b = a;
    b.position  = Dia::Maths::Vector2D(0.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.position.x, 10.0f);
    EXPECT_FLOAT_EQ(a.position.y, 20.0f);
}

// ---------------------------------------------------------------------------
// Registry CRUD
// ---------------------------------------------------------------------------

TEST(DiaLighting2D_Registry, Register_Has_Finds)
{
    LightRegistry2D reg;
    PointLight2D light;
    reg.Register(Dia::Core::StringCRC("sun"), light);
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("sun")));
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("moon")));
}

TEST(DiaLighting2D_Registry, Register_IncrementsCount)
{
    LightRegistry2D reg;
    EXPECT_EQ(reg.GetCount(), 0u);
    reg.Register(Dia::Core::StringCRC("a"), PointLight2D{});
    EXPECT_EQ(reg.GetCount(), 1u);
    reg.Register(Dia::Core::StringCRC("b"), PointLight2D{});
    EXPECT_EQ(reg.GetCount(), 2u);
}

TEST(DiaLighting2D_Registry, Unregister_DecreasesCount)
{
    LightRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("a"), PointLight2D{});
    reg.Register(Dia::Core::StringCRC("b"), PointLight2D{});
    reg.Unregister(Dia::Core::StringCRC("a"));
    EXPECT_EQ(reg.GetCount(), 1u);
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("a")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("b")));
}

TEST(DiaLighting2D_Registry, Unregister_LastEntry_CountBecomesZero)
{
    LightRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("only"), PointLight2D{});
    reg.Unregister(Dia::Core::StringCRC("only"));
    EXPECT_EQ(reg.GetCount(), 0u);
}

TEST(DiaLighting2D_Registry, Get_ReturnsCorrectLight)
{
    LightRegistry2D reg;
    PointLight2D light;
    light.radius = 300.0f;
    reg.Register(Dia::Core::StringCRC("torch"), light);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("torch")).radius, 300.0f);
}

TEST(DiaLighting2D_Registry, Get_MutateViaReference)
{
    LightRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("lamp"), PointLight2D{});
    reg.Get(Dia::Core::StringCRC("lamp")).intensity = 0.5f;
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("lamp")).intensity, 0.5f);
}

// ---------------------------------------------------------------------------
// Indexed iteration
// ---------------------------------------------------------------------------

TEST(DiaLighting2D_Registry, GetByIndex_IteratesAllLights)
{
    LightRegistry2D reg;
    PointLight2D a; a.radius = 10.0f;
    PointLight2D b; b.radius = 20.0f;
    PointLight2D c; c.radius = 30.0f;
    reg.Register(Dia::Core::StringCRC("a"), a);
    reg.Register(Dia::Core::StringCRC("b"), b);
    reg.Register(Dia::Core::StringCRC("c"), c);

    float sum = 0.0f;
    for (unsigned int i = 0; i < reg.GetCount(); ++i)
        sum += reg.GetByIndex(i).radius;
    EXPECT_FLOAT_EQ(sum, 60.0f);
}

// ---------------------------------------------------------------------------
// GetLightsForLayer
// ---------------------------------------------------------------------------

TEST(DiaLighting2D_Registry, GetLightsForLayer_FiltersByMask)
{
    // Light 0: layerMask = 0b0001 (bit 0 only)
    // Light 1: layerMask = 0b0110 (bits 1 and 2)
    // Light 2: layerMask = 0xFFFFFFFF (all layers)
    LightRegistry2D reg;

    PointLight2D l0; l0.layerMask = 0x1u;
    PointLight2D l1; l1.layerMask = 0x6u;
    PointLight2D l2; l2.layerMask = 0xFFFFFFFFu;

    reg.Register(Dia::Core::StringCRC("l0"), l0);
    reg.Register(Dia::Core::StringCRC("l1"), l1);
    reg.Register(Dia::Core::StringCRC("l2"), l2);

    DynamicArrayLightPtrs out;

    out.RemoveAll();
    reg.GetLightsForLayer(0, out);  // bit 0 → l0, l2
    EXPECT_EQ(out.Size(), 2u);

    out.RemoveAll();
    reg.GetLightsForLayer(1, out);  // bit 1 → l1, l2
    EXPECT_EQ(out.Size(), 2u);

    out.RemoveAll();
    reg.GetLightsForLayer(2, out);  // bit 2 → l1, l2
    EXPECT_EQ(out.Size(), 2u);

    out.RemoveAll();
    reg.GetLightsForLayer(3, out);  // bit 3 → only l2
    EXPECT_EQ(out.Size(), 1u);
}

TEST(DiaLighting2D_Registry, GetLightsForLayer_DisabledLightsExcluded)
{
    LightRegistry2D reg;
    PointLight2D enabled; enabled.layerMask = 0xFFFFFFFFu; enabled.enabled = true;
    PointLight2D disabled; disabled.layerMask = 0xFFFFFFFFu; disabled.enabled = false;
    reg.Register(Dia::Core::StringCRC("on"),  enabled);
    reg.Register(Dia::Core::StringCRC("off"), disabled);

    DynamicArrayLightPtrs out;
    reg.GetLightsForLayer(0, out);
    EXPECT_EQ(out.Size(), 1u);
    EXPECT_FLOAT_EQ(out[0]->intensity, 1.0f);
}

TEST(DiaLighting2D_Registry, GetLightsForLayer_EmptyRegistry_ReturnsEmpty)
{
    LightRegistry2D reg;
    DynamicArrayLightPtrs out;
    reg.GetLightsForLayer(0, out);
    EXPECT_EQ(out.Size(), 0u);
}

// ---------------------------------------------------------------------------
// LightBuilder
// ---------------------------------------------------------------------------

TEST(DiaLighting2D_LightBuilder, WithLight_RegistersInRegistry)
{
    auto builder = Dia::Lighting2D::Testing::LightBuilder{}
        .WithLight("campfire", 100.0f, 200.0f, 150.0f, 0.8f);

    LightRegistry2D& reg = builder.Registry();
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("campfire")));
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("campfire")).radius, 150.0f);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("campfire")).intensity, 0.8f);
}

TEST(DiaLighting2D_LightBuilder, OnLayers_SetsMask)
{
    auto builder = Dia::Lighting2D::Testing::LightBuilder{}
        .WithLight("torch")
        .OnLayers(0x3u);

    EXPECT_EQ(builder.Registry().Get(Dia::Core::StringCRC("torch")).layerMask, 0x3u);
}

TEST(DiaLighting2D_LightBuilder, Disabled_SetsEnabledFalse)
{
    auto builder = Dia::Lighting2D::Testing::LightBuilder{}
        .WithLight("ghost")
        .Disabled();

    EXPECT_FALSE(builder.Registry().Get(Dia::Core::StringCRC("ghost")).enabled);
}

TEST(DiaLighting2D_LightBuilder, MultipleChained_AllRegistered)
{
    auto builder = Dia::Lighting2D::Testing::LightBuilder{}
        .WithLight("a")
        .WithLight("b")
        .WithLight("c");

    EXPECT_EQ(builder.Registry().GetCount(), 3u);
}

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

TEST(DiaLighting2D_Registry, MaxCapacity_FillsWithoutAssert)
{
    LightRegistry2D reg;
    char name[8];
    for (unsigned int i = 0; i < LightRegistry2D::kMaxLights; ++i)
    {
        name[0] = 'a' + static_cast<char>(i);
        name[1] = '\0';
        reg.Register(Dia::Core::StringCRC(name), PointLight2D{});
    }
    EXPECT_EQ(reg.GetCount(), LightRegistry2D::kMaxLights);
}
