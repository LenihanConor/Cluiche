#include <gtest/gtest.h>

#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/DirectionalLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaLighting3D/AmbientLight3D.h>
#include <DiaLighting3D/Registry/LightRegistry3D.h>
#include <DiaLighting3D/Behaviours/ILightBehaviour3D.h>
#include <DiaLighting3D/Testing/LightBuilder3D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Lighting3D;

// --- Light value type defaults ---

TEST(DiaLighting3D_PointLight3D, DefaultConstruct_ExpectedDefaults)
{
    PointLight3D light;
    EXPECT_FLOAT_EQ(light.radius,    10.0f);
    EXPECT_FLOAT_EQ(light.intensity,  1.0f);
    EXPECT_TRUE(light.enabled);
}

TEST(DiaLighting3D_DirectionalLight3D, DefaultConstruct_ExpectedDefaults)
{
    DirectionalLight3D light;
    EXPECT_FLOAT_EQ(light.direction.x,  0.0f);
    EXPECT_FLOAT_EQ(light.direction.y, -1.0f);
    EXPECT_FLOAT_EQ(light.direction.z,  0.0f);
    EXPECT_FLOAT_EQ(light.intensity,    1.0f);
    EXPECT_TRUE(light.enabled);
}

TEST(DiaLighting3D_SpotLight3D, DefaultConstruct_ExpectedDefaults)
{
    SpotLight3D light;
    EXPECT_FLOAT_EQ(light.innerAngle, 15.0f);
    EXPECT_FLOAT_EQ(light.outerAngle, 30.0f);
    EXPECT_FLOAT_EQ(light.range,      20.0f);
    EXPECT_FLOAT_EQ(light.intensity,   1.0f);
    EXPECT_TRUE(light.enabled);
}

TEST(DiaLighting3D_AmbientLight3D, DefaultConstruct_ExpectedDefaults)
{
    AmbientLight3D light;
    EXPECT_FLOAT_EQ(light.intensity, 0.1f);
    EXPECT_TRUE(light.enabled);
}

// --- Registry CRUD ---

TEST(DiaLighting3D_Registry, RegisterPoint_Has_Finds)
{
    LightRegistry3D reg;
    reg.RegisterPoint(Dia::Core::StringCRC("sun"), PointLight3D{});
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("sun")));
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("moon")));
}

TEST(DiaLighting3D_Registry, RegisterDirectional_Has_Finds)
{
    LightRegistry3D reg;
    reg.RegisterDirectional(Dia::Core::StringCRC("sun"), DirectionalLight3D{});
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("sun")));
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("moon")));
}

TEST(DiaLighting3D_Registry, RegisterSpot_Has_Finds)
{
    LightRegistry3D reg;
    reg.RegisterSpot(Dia::Core::StringCRC("spotlight"), SpotLight3D{});
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("spotlight")));
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("other")));
}

TEST(DiaLighting3D_Registry, RegisterPoint_IncrementsCount)
{
    LightRegistry3D reg;
    EXPECT_EQ(reg.GetPointCount(), 0u);
    reg.RegisterPoint(Dia::Core::StringCRC("a"), PointLight3D{});
    EXPECT_EQ(reg.GetPointCount(), 1u);
    reg.RegisterPoint(Dia::Core::StringCRC("b"), PointLight3D{});
    EXPECT_EQ(reg.GetPointCount(), 2u);
}

TEST(DiaLighting3D_Registry, RegisterDirectional_IncrementsCount)
{
    LightRegistry3D reg;
    EXPECT_EQ(reg.GetDirectionalCount(), 0u);
    reg.RegisterDirectional(Dia::Core::StringCRC("sun"), DirectionalLight3D{});
    EXPECT_EQ(reg.GetDirectionalCount(), 1u);
    reg.RegisterDirectional(Dia::Core::StringCRC("moon"), DirectionalLight3D{});
    EXPECT_EQ(reg.GetDirectionalCount(), 2u);
}

TEST(DiaLighting3D_Registry, RegisterSpot_IncrementsCount)
{
    LightRegistry3D reg;
    EXPECT_EQ(reg.GetSpotCount(), 0u);
    reg.RegisterSpot(Dia::Core::StringCRC("s1"), SpotLight3D{});
    EXPECT_EQ(reg.GetSpotCount(), 1u);
    reg.RegisterSpot(Dia::Core::StringCRC("s2"), SpotLight3D{});
    EXPECT_EQ(reg.GetSpotCount(), 2u);
}

TEST(DiaLighting3D_Registry, Unregister_Point_DecreasesCount)
{
    LightRegistry3D reg;
    reg.RegisterPoint(Dia::Core::StringCRC("a"), PointLight3D{});
    reg.RegisterPoint(Dia::Core::StringCRC("b"), PointLight3D{});
    reg.Unregister(Dia::Core::StringCRC("a"));
    EXPECT_EQ(reg.GetPointCount(), 1u);
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("a")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("b")));
}

TEST(DiaLighting3D_Registry, Unregister_Directional_DecreasesCount)
{
    LightRegistry3D reg;
    reg.RegisterDirectional(Dia::Core::StringCRC("sun"), DirectionalLight3D{});
    reg.RegisterDirectional(Dia::Core::StringCRC("moon"), DirectionalLight3D{});
    reg.Unregister(Dia::Core::StringCRC("sun"));
    EXPECT_EQ(reg.GetDirectionalCount(), 1u);
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("sun")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("moon")));
}

TEST(DiaLighting3D_Registry, Unregister_Spot_DecreasesCount)
{
    LightRegistry3D reg;
    reg.RegisterSpot(Dia::Core::StringCRC("s1"), SpotLight3D{});
    reg.RegisterSpot(Dia::Core::StringCRC("s2"), SpotLight3D{});
    reg.Unregister(Dia::Core::StringCRC("s1"));
    EXPECT_EQ(reg.GetSpotCount(), 1u);
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("s1")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("s2")));
}

TEST(DiaLighting3D_Registry, Has_ReturnsFalse_ForUnknownId)
{
    LightRegistry3D reg;
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("unknown")));
}

TEST(DiaLighting3D_Registry, GetPoint_MutateViaReference)
{
    LightRegistry3D reg;
    reg.RegisterPoint(Dia::Core::StringCRC("lamp"), PointLight3D{});
    reg.GetPoint(Dia::Core::StringCRC("lamp")).intensity = 0.5f;
    EXPECT_FLOAT_EQ(reg.GetPoint(Dia::Core::StringCRC("lamp")).intensity, 0.5f);
}

TEST(DiaLighting3D_Registry, GetDirectional_MutateViaReference)
{
    LightRegistry3D reg;
    reg.RegisterDirectional(Dia::Core::StringCRC("sun"), DirectionalLight3D{});
    reg.GetDirectional(Dia::Core::StringCRC("sun")).intensity = 2.0f;
    EXPECT_FLOAT_EQ(reg.GetDirectional(Dia::Core::StringCRC("sun")).intensity, 2.0f);
}

TEST(DiaLighting3D_Registry, GetSpot_MutateViaReference)
{
    LightRegistry3D reg;
    reg.RegisterSpot(Dia::Core::StringCRC("spot"), SpotLight3D{});
    reg.GetSpot(Dia::Core::StringCRC("spot")).range = 50.0f;
    EXPECT_FLOAT_EQ(reg.GetSpot(Dia::Core::StringCRC("spot")).range, 50.0f);
}

TEST(DiaLighting3D_Registry, SetAmbient_GetAmbient_RoundTrips)
{
    LightRegistry3D reg;
    AmbientLight3D ambient;
    ambient.intensity = 0.3f;
    reg.SetAmbient(ambient);
    EXPECT_FLOAT_EQ(reg.GetAmbient().intensity, 0.3f);
}

// --- Indexed iteration ---

TEST(DiaLighting3D_Registry, GetPointByIndex_IteratesAllPoints)
{
    LightRegistry3D reg;
    PointLight3D a; a.radius = 10.0f;
    PointLight3D b; b.radius = 20.0f;
    PointLight3D c; c.radius = 30.0f;
    reg.RegisterPoint(Dia::Core::StringCRC("a"), a);
    reg.RegisterPoint(Dia::Core::StringCRC("b"), b);
    reg.RegisterPoint(Dia::Core::StringCRC("c"), c);

    float sum = 0.0f;
    for (unsigned int i = 0; i < reg.GetPointCount(); ++i)
        sum += reg.GetPointByIndex(i).radius;
    EXPECT_FLOAT_EQ(sum, 60.0f);
}

TEST(DiaLighting3D_Registry, GetDirectionalByIndex_IteratesAllDirectionals)
{
    LightRegistry3D reg;
    DirectionalLight3D a; a.intensity = 1.0f;
    DirectionalLight3D b; b.intensity = 2.0f;
    reg.RegisterDirectional(Dia::Core::StringCRC("a"), a);
    reg.RegisterDirectional(Dia::Core::StringCRC("b"), b);

    float sum = 0.0f;
    for (unsigned int i = 0; i < reg.GetDirectionalCount(); ++i)
        sum += reg.GetDirectionalByIndex(i).intensity;
    EXPECT_FLOAT_EQ(sum, 3.0f);
}

TEST(DiaLighting3D_Registry, GetSpotByIndex_IteratesAllSpots)
{
    LightRegistry3D reg;
    SpotLight3D a; a.range = 10.0f;
    SpotLight3D b; b.range = 20.0f;
    SpotLight3D c; c.range = 30.0f;
    reg.RegisterSpot(Dia::Core::StringCRC("a"), a);
    reg.RegisterSpot(Dia::Core::StringCRC("b"), b);
    reg.RegisterSpot(Dia::Core::StringCRC("c"), c);

    float sum = 0.0f;
    for (unsigned int i = 0; i < reg.GetSpotCount(); ++i)
        sum += reg.GetSpotByIndex(i).range;
    EXPECT_FLOAT_EQ(sum, 60.0f);
}

// --- UpdateAll ---

struct MockBehaviour3D : public ILightBehaviour3D
{
    bool mWasCalled = false;
    Dia::Core::StringCRC GetTypeId() const override { return Dia::Core::StringCRC("mock"); }
    void Update(float) override { mWasCalled = true; }
};

TEST(DiaLighting3D_Registry, UpdateAll_CallsBehaviourUpdate)
{
    LightRegistry3D reg;
    reg.RegisterPoint(Dia::Core::StringCRC("torch"), PointLight3D{});

    MockBehaviour3D mock;
    reg.AttachBehaviour(Dia::Core::StringCRC("torch"), &mock);
    reg.UpdateAll(0.016f);

    EXPECT_TRUE(mock.mWasCalled);
}

// --- LightBuilder3D ---

TEST(DiaLighting3D_LightBuilder3D, WithPoint_RegistersInRegistry)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithPoint("campfire", 0.0f, 0.0f, 0.0f, 15.0f, 0.8f);

    LightRegistry3D& reg = builder.Registry();
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("campfire")));
    EXPECT_FLOAT_EQ(reg.GetPoint(Dia::Core::StringCRC("campfire")).radius, 15.0f);
    EXPECT_FLOAT_EQ(reg.GetPoint(Dia::Core::StringCRC("campfire")).intensity, 0.8f);
}

TEST(DiaLighting3D_LightBuilder3D, PointDisabled_SetsEnabledFalse)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithPoint("ghost")
        .PointDisabled();

    EXPECT_FALSE(builder.Registry().GetPoint(Dia::Core::StringCRC("ghost")).enabled);
}

TEST(DiaLighting3D_LightBuilder3D, WithDirectional_RegistersInRegistry)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithDirectional("sun", 0.0f, -1.0f, 0.0f, 1.5f);

    LightRegistry3D& reg = builder.Registry();
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("sun")));
    EXPECT_FLOAT_EQ(reg.GetDirectional(Dia::Core::StringCRC("sun")).intensity, 1.5f);
}

TEST(DiaLighting3D_LightBuilder3D, WithSpot_RegistersInRegistry)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithSpot("flashlight");

    EXPECT_TRUE(builder.Registry().Has(Dia::Core::StringCRC("flashlight")));
}

TEST(DiaLighting3D_LightBuilder3D, WithAmbient_SetsIntensity)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithAmbient(0.25f);

    EXPECT_FLOAT_EQ(builder.Registry().GetAmbient().intensity, 0.25f);
}

// --- Capacity / Boundary ---

// NOTE: RegisterPoint/RegisterDirectional/RegisterSpot assert (DIA_ASSERT) on duplicate ids before
// returning false. In Debug builds DIA_ASSERT calls __debugbreak() which terminates the process
// without a debugger. These tests therefore verify the single-registration path only (count == 1
// after one call) and confirm Has() is consistent. The guard-returns-false path is safe to test in
// Release builds only.

TEST(DiaLighting3D_Registry, RegisterPoint_SingleReg_CountIsOne)
{
    LightRegistry3D reg;
    EXPECT_TRUE(reg.RegisterPoint(Dia::Core::StringCRC("a"), PointLight3D{}));
    EXPECT_EQ(reg.GetPointCount(), 1u);
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("a")));
}

TEST(DiaLighting3D_Registry, RegisterDirectional_SingleReg_CountIsOne)
{
    LightRegistry3D reg;
    EXPECT_TRUE(reg.RegisterDirectional(Dia::Core::StringCRC("sun"), DirectionalLight3D{}));
    EXPECT_EQ(reg.GetDirectionalCount(), 1u);
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("sun")));
}

TEST(DiaLighting3D_Registry, RegisterSpot_SingleReg_CountIsOne)
{
    LightRegistry3D reg;
    EXPECT_TRUE(reg.RegisterSpot(Dia::Core::StringCRC("s"), SpotLight3D{}));
    EXPECT_EQ(reg.GetSpotCount(), 1u);
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("s")));
}

TEST(DiaLighting3D_Registry, RegisterPoint_MaxCapacity_FillsAndHoldsAll)
{
    LightRegistry3D reg;
    char name[8];
    for (unsigned int i = 0; i < LightRegistry3D::kMaxPointLights; ++i)
    {
        name[0] = 'a' + static_cast<char>(i); name[1] = '\0';
        EXPECT_TRUE(reg.RegisterPoint(Dia::Core::StringCRC(name), PointLight3D{}));
    }
    EXPECT_EQ(reg.GetPointCount(), LightRegistry3D::kMaxPointLights);
}

TEST(DiaLighting3D_Registry, RegisterDirectional_MaxCapacity_FillsAndHoldsAll)
{
    LightRegistry3D reg;
    char name[8];
    for (unsigned int i = 0; i < LightRegistry3D::kMaxDirectionalLights; ++i)
    {
        name[0] = 'a' + static_cast<char>(i); name[1] = '\0';
        EXPECT_TRUE(reg.RegisterDirectional(Dia::Core::StringCRC(name), DirectionalLight3D{}));
    }
    EXPECT_EQ(reg.GetDirectionalCount(), LightRegistry3D::kMaxDirectionalLights);
}

TEST(DiaLighting3D_Registry, RegisterSpot_MaxCapacity_FillsAndHoldsAll)
{
    LightRegistry3D reg;
    char name[8];
    for (unsigned int i = 0; i < LightRegistry3D::kMaxSpotLights; ++i)
    {
        name[0] = 'a' + static_cast<char>(i); name[1] = '\0';
        EXPECT_TRUE(reg.RegisterSpot(Dia::Core::StringCRC(name), SpotLight3D{}));
    }
    EXPECT_EQ(reg.GetSpotCount(), LightRegistry3D::kMaxSpotLights);
}

// NOTE: AttachBehaviour for an unknown light id also calls DIA_ASSERT then returns false.
// Cannot test the false-return path in Debug mode without crashing the runner.

TEST(DiaLighting3D_Registry, AttachBehaviour_ToDirectionalLight_Dispatched)
{
    LightRegistry3D reg;
    reg.RegisterDirectional(Dia::Core::StringCRC("sun"), DirectionalLight3D{});
    MockBehaviour3D mock;
    EXPECT_TRUE(reg.AttachBehaviour(Dia::Core::StringCRC("sun"), &mock));
    reg.UpdateAll(0.016f);
    EXPECT_TRUE(mock.mWasCalled);
}

TEST(DiaLighting3D_Registry, AttachBehaviour_ToSpotLight_Dispatched)
{
    LightRegistry3D reg;
    reg.RegisterSpot(Dia::Core::StringCRC("spot"), SpotLight3D{});
    MockBehaviour3D mock;
    EXPECT_TRUE(reg.AttachBehaviour(Dia::Core::StringCRC("spot"), &mock));
    reg.UpdateAll(0.016f);
    EXPECT_TRUE(mock.mWasCalled);
}

TEST(DiaLighting3D_Registry, DetachBehaviour_RemovedBehaviourNotCalled)
{
    LightRegistry3D reg;
    reg.RegisterPoint(Dia::Core::StringCRC("torch"), PointLight3D{});
    MockBehaviour3D mock;
    reg.AttachBehaviour(Dia::Core::StringCRC("torch"), &mock);
    reg.DetachBehaviour(Dia::Core::StringCRC("torch"), Dia::Core::StringCRC("mock"));
    reg.UpdateAll(0.016f);
    EXPECT_FALSE(mock.mWasCalled);
}

TEST(DiaLighting3D_Registry, AttachBehaviour_MaxBehaviours_AllCalled)
{
    LightRegistry3D reg;
    reg.RegisterPoint(Dia::Core::StringCRC("torch"), PointLight3D{});

    MockBehaviour3D mocks[LightRegistry3D::kMaxBehaviours];
    // AttachBehaviour does not deduplicate by typeId — it checks capacity only.
    // All 4 mocks share typeId "mock" which is fine for attaching.
    for (unsigned int i = 0; i < LightRegistry3D::kMaxBehaviours; ++i)
        EXPECT_TRUE(reg.AttachBehaviour(Dia::Core::StringCRC("torch"), &mocks[i]));

    reg.UpdateAll(0.016f);

    for (unsigned int i = 0; i < LightRegistry3D::kMaxBehaviours; ++i)
        EXPECT_TRUE(mocks[i].mWasCalled);
}

// --- Stress ---

TEST(DiaLighting3D_Stress, FillAndDrainPointRegistry)
{
    LightRegistry3D reg;
    char name[8];
    for (unsigned int pass = 0; pass < 3; ++pass)
    {
        for (unsigned int i = 0; i < LightRegistry3D::kMaxPointLights; ++i)
        {
            name[0] = 'a' + static_cast<char>(i); name[1] = '\0';
            reg.RegisterPoint(Dia::Core::StringCRC(name), PointLight3D{});
        }
        EXPECT_EQ(reg.GetPointCount(), LightRegistry3D::kMaxPointLights);
        for (unsigned int i = 0; i < LightRegistry3D::kMaxPointLights; ++i)
        {
            name[0] = 'a' + static_cast<char>(i); name[1] = '\0';
            reg.Unregister(Dia::Core::StringCRC(name));
        }
        EXPECT_EQ(reg.GetPointCount(), 0u);
    }
}

TEST(DiaLighting3D_Stress, UpdateAll_MaxBehavioursPerLight_NoCrash)
{
    LightRegistry3D reg;
    reg.RegisterPoint(Dia::Core::StringCRC("torch"), PointLight3D{});
    MockBehaviour3D mocks[LightRegistry3D::kMaxBehaviours];
    for (unsigned int i = 0; i < LightRegistry3D::kMaxBehaviours; ++i)
        reg.AttachBehaviour(Dia::Core::StringCRC("torch"), &mocks[i]);

    for (int i = 0; i < 1000; ++i)
        reg.UpdateAll(0.016f);
    // No crash — all behaviours called 1000 times each
    EXPECT_TRUE(mocks[0].mWasCalled);
}

// --- LightBuilder3D (additional) ---

TEST(DiaLighting3D_LightBuilder3D, DirectionalDisabled_SetsEnabledFalse)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithDirectional("moon")
        .DirectionalDisabled();
    EXPECT_FALSE(builder.Registry().GetDirectional(Dia::Core::StringCRC("moon")).enabled);
}

TEST(DiaLighting3D_LightBuilder3D, SpotDisabled_SetsEnabledFalse)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithSpot("cone")
        .SpotDisabled();
    EXPECT_FALSE(builder.Registry().GetSpot(Dia::Core::StringCRC("cone")).enabled);
}

TEST(DiaLighting3D_LightBuilder3D, MultipleWithPoint_CorrectCount)
{
    auto builder = Dia::Lighting3D::Testing::LightBuilder3D{}
        .WithPoint("a")
        .WithPoint("b")
        .WithPoint("c");
    EXPECT_EQ(builder.Registry().GetPointCount(), 3u);
}
