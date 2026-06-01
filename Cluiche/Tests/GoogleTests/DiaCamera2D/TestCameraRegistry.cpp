#include <gtest/gtest.h>

#include <DiaCamera2D/Camera2D.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaCamera2D/Testing/CameraBuilder.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Camera2D;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Camera2D, DefaultConstruct_OriginZoom1NoRotation)
{
    Camera2D cam;
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 1.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(), 0.0f);
}

TEST(DiaCamera2D_Camera2D, ExplicitConstruct_ValuesMatch)
{
    Camera2D cam(Dia::Maths::Vector2D(100.0f, 200.0f), 2.5f, 45.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 100.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 200.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 2.5f);
    EXPECT_FLOAT_EQ(cam.GetRotation(), 45.0f);
}

TEST(DiaCamera2D_Camera2D, SetPosition_UpdatesPosition)
{
    Camera2D cam;
    cam.SetPosition(Dia::Maths::Vector2D(50.0f, -30.0f));
    EXPECT_FLOAT_EQ(cam.GetPosition().x,  50.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, -30.0f);
}

TEST(DiaCamera2D_Camera2D, SetZoom_UpdatesZoom)
{
    Camera2D cam;
    cam.SetZoom(3.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 3.0f);
}

TEST(DiaCamera2D_Camera2D, CopySemantics_IndependentValues)
{
    Camera2D a(Dia::Maths::Vector2D(10.0f, 20.0f), 2.0f, 90.0f);
    Camera2D b = a;
    b.SetPosition(Dia::Maths::Vector2D(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(a.GetPosition().x, 10.0f); // a unchanged
}

// ---------------------------------------------------------------------------
// Registry CRUD
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Registry, Register_Has_Finds)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("main"), Camera2D{});
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("main")));
    EXPECT_EQ(reg.GetCount(), 1u);
}

TEST(DiaCamera2D_Registry, Register_Multiple_AllFound)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("a"), Camera2D{});
    reg.Register(Dia::Core::StringCRC("b"), Camera2D{});
    reg.Register(Dia::Core::StringCRC("c"), Camera2D{});
    EXPECT_EQ(reg.GetCount(), 3u);
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("a")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("b")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("c")));
}

TEST(DiaCamera2D_Registry, Has_UnknownId_ReturnsFalse)
{
    CameraRegistry2D reg;
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("nope")));
}

TEST(DiaCamera2D_Registry, Get_ReturnsCorrectCamera)
{
    CameraRegistry2D reg;
    Camera2D cam(Dia::Maths::Vector2D(42.0f, 7.0f), 1.5f);
    reg.Register(Dia::Core::StringCRC("cam"), cam);

    const Camera2D& got = reg.Get(Dia::Core::StringCRC("cam"));
    EXPECT_FLOAT_EQ(got.GetPosition().x, 42.0f);
    EXPECT_FLOAT_EQ(got.GetZoom(), 1.5f);
}

TEST(DiaCamera2D_Registry, Get_MutatesInPlace)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera2D{});
    reg.Get(Dia::Core::StringCRC("cam")).SetZoom(3.0f);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("cam")).GetZoom(), 3.0f);
}

TEST(DiaCamera2D_Registry, Unregister_RemovesCamera)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("x"), Camera2D{});
    reg.Register(Dia::Core::StringCRC("y"), Camera2D{});
    reg.Unregister(Dia::Core::StringCRC("x"));
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("x")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("y")));
    EXPECT_EQ(reg.GetCount(), 1u);
}

TEST(DiaCamera2D_Registry, Unregister_LastCamera_EmptyRegistry)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("only"), Camera2D{});
    reg.Unregister(Dia::Core::StringCRC("only"));
    EXPECT_EQ(reg.GetCount(), 0u);
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("only")));
}

// ---------------------------------------------------------------------------
// Active camera
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Registry, SetActive_GetActive_ReturnsCorrectCamera)
{
    CameraRegistry2D reg;
    Camera2D a(Dia::Maths::Vector2D(1.0f, 0.0f));
    Camera2D b(Dia::Maths::Vector2D(2.0f, 0.0f));
    reg.Register(Dia::Core::StringCRC("a"), a);
    reg.Register(Dia::Core::StringCRC("b"), b);
    reg.SetActive(Dia::Core::StringCRC("b"));
    EXPECT_FLOAT_EQ(reg.GetActive().GetPosition().x, 2.0f);
}

TEST(DiaCamera2D_Registry, SetActive_SwitchActive_ReturnsNewCamera)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("a"), Camera2D(Dia::Maths::Vector2D(1.0f, 0.0f)));
    reg.Register(Dia::Core::StringCRC("b"), Camera2D(Dia::Maths::Vector2D(2.0f, 0.0f)));
    reg.SetActive(Dia::Core::StringCRC("a"));
    reg.SetActive(Dia::Core::StringCRC("b"));
    EXPECT_EQ(reg.GetActiveId(), Dia::Core::StringCRC("b"));
}

TEST(DiaCamera2D_Registry, GetActiveId_ReturnsActiveId)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("gameplay"), Camera2D{});
    reg.SetActive(Dia::Core::StringCRC("gameplay"));
    EXPECT_EQ(reg.GetActiveId(), Dia::Core::StringCRC("gameplay"));
}

// ---------------------------------------------------------------------------
// UpdateAll with no behaviours (baseline)
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Registry, UpdateAll_NoBehaviours_CameraUnchanged)
{
    CameraRegistry2D reg;
    Camera2D cam(Dia::Maths::Vector2D(5.0f, 10.0f));
    reg.Register(Dia::Core::StringCRC("c"), cam);
    reg.UpdateAll(1.0f / 60.0f);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("c")).GetPosition().x, 5.0f);
}

TEST(DiaCamera2D_Registry, UpdateAll_ZeroDt_NoCrash)
{
    CameraRegistry2D reg;
    reg.Register(Dia::Core::StringCRC("c"), Camera2D{});
    reg.UpdateAll(0.0f);  // must not crash or divide by zero
    EXPECT_EQ(reg.GetCount(), 1u);
}

// ---------------------------------------------------------------------------
// Builder utility
// ---------------------------------------------------------------------------

TEST(DiaCamera2D_Builder, BuilderCreatesAndActivates)
{
    Testing::CameraBuilder builder;
    builder.WithCamera("gameplay", 100.0f, 200.0f, 1.5f).AsActive();

    CameraRegistry2D& reg = builder.Registry();
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("gameplay")));
    EXPECT_EQ(reg.GetActiveId(), Dia::Core::StringCRC("gameplay"));
    EXPECT_FLOAT_EQ(reg.GetActive().GetPosition().x, 100.0f);
}
