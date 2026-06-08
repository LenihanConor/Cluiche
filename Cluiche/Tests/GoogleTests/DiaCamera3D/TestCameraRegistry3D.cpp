#include <gtest/gtest.h>

#include <DiaCamera3D/Registry/CameraRegistry3D.h>
#include <DiaCamera3D/Registry/ICameraBehaviour3D.h>
#include <DiaCamera3D/Testing/CameraBuilder3D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Camera3D;

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Registry, Register_Has_ReturnsTrue)
{
    CameraRegistry3D reg;
    EXPECT_TRUE(reg.Register(Dia::Core::StringCRC("main"), Camera3D{}));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("main")));
}

TEST(DiaCamera3D_Registry, Register_Duplicate_ReturnsFalse)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera3D{});
    EXPECT_FALSE(reg.Register(Dia::Core::StringCRC("cam"), Camera3D{}));
}

TEST(DiaCamera3D_Registry, Register_IncreasesCount)
{
    CameraRegistry3D reg;
    EXPECT_EQ(reg.GetCount(), 0u);
    reg.Register(Dia::Core::StringCRC("a"), Camera3D{});
    EXPECT_EQ(reg.GetCount(), 1u);
    reg.Register(Dia::Core::StringCRC("b"), Camera3D{});
    EXPECT_EQ(reg.GetCount(), 2u);
}

TEST(DiaCamera3D_Registry, Unregister_DecreasesCount)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("a"), Camera3D{});
    reg.Register(Dia::Core::StringCRC("b"), Camera3D{});
    EXPECT_EQ(reg.GetCount(), 2u);
    reg.Unregister(Dia::Core::StringCRC("a"));
    EXPECT_EQ(reg.GetCount(), 1u);
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("a")));
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("b")));
}

TEST(DiaCamera3D_Registry, Unregister_NotFound_NoEffect)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera3D{});
    reg.Unregister(Dia::Core::StringCRC("nonexistent"));
    EXPECT_EQ(reg.GetCount(), 1u);
}

TEST(DiaCamera3D_Registry, Has_ReturnsFalse_ForUnknown)
{
    CameraRegistry3D reg;
    EXPECT_FALSE(reg.Has(Dia::Core::StringCRC("unknown_xyz")));
}

// ---------------------------------------------------------------------------
// Active camera
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Registry, SetActive_GetActive_Roundtrips)
{
    CameraRegistry3D reg;
    Camera3D cam;
    cam.perspective.fovY = 45.0f;
    reg.Register(Dia::Core::StringCRC("main"), cam);
    reg.SetActive(Dia::Core::StringCRC("main"));

    EXPECT_FLOAT_EQ(reg.GetActive().perspective.fovY, 45.0f);
}

TEST(DiaCamera3D_Registry, GetActive_MutateViaReference)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera3D{});
    reg.SetActive(Dia::Core::StringCRC("cam"));

    reg.GetActive().perspective.farZ = 500.0f;
    EXPECT_FLOAT_EQ(reg.GetActive().perspective.farZ, 500.0f);
}

// ---------------------------------------------------------------------------
// Direct access
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Registry, Get_MutatesPosition)
{
    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera3D{});
    reg.Get(Dia::Core::StringCRC("cam")).position = Dia::Maths::Vector3D(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("cam")).position.x, 1.0f);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("cam")).position.y, 2.0f);
    EXPECT_FLOAT_EQ(reg.Get(Dia::Core::StringCRC("cam")).position.z, 3.0f);
}

// ---------------------------------------------------------------------------
// Behaviour management
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Registry, AttachBehaviour_UpdateAll_Dispatched)
{
    // Registry owns behaviour pointers (deletes on destruction) — must heap-allocate.
    // We track calls via a shared counter that outlives the behaviour.
    int callCount = 0;
    struct CountingBehaviour : public ICameraBehaviour3D
    {
        int* mCounter;
        explicit CountingBehaviour(int* counter) : mCounter(counter) {}
        Dia::Core::StringCRC GetTypeId() const override { return Dia::Core::StringCRC("CountingBehaviour"); }
        void Update(Camera3D& /*cam*/, float /*dt*/) override { ++(*mCounter); }
    };

    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera3D{});
    EXPECT_TRUE(reg.AttachBehaviour(Dia::Core::StringCRC("cam"), new CountingBehaviour(&callCount)));
    reg.UpdateAll(0.016f);
    EXPECT_EQ(callCount, 1);
    // reg destructor deletes the behaviour
}

TEST(DiaCamera3D_Registry, DetachBehaviour_NotCalled)
{
    // Attach then detach (DetachBehaviour deletes the pointer). UpdateAll must not call it.
    int callCount = 0;
    struct CountingBehaviour : public ICameraBehaviour3D
    {
        int* mCounter;
        explicit CountingBehaviour(int* counter) : mCounter(counter) {}
        Dia::Core::StringCRC GetTypeId() const override { return Dia::Core::StringCRC("CountingBehaviour"); }
        void Update(Camera3D& /*cam*/, float /*dt*/) override { ++(*mCounter); }
    };

    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("cam"), Camera3D{});
    reg.AttachBehaviour(Dia::Core::StringCRC("cam"), new CountingBehaviour(&callCount));
    reg.DetachBehaviour(Dia::Core::StringCRC("cam"), Dia::Core::StringCRC("CountingBehaviour"));
    reg.UpdateAll(0.016f);
    EXPECT_EQ(callCount, 0);
}

TEST(DiaCamera3D_Registry, UpdateAll_MultipleCamera_AllBehavioursCalled)
{
    int count1 = 0;
    int count2 = 0;
    struct CountingBehaviour : public ICameraBehaviour3D
    {
        int* mCounter;
        explicit CountingBehaviour(int* counter) : mCounter(counter) {}
        Dia::Core::StringCRC GetTypeId() const override { return Dia::Core::StringCRC("CountingBehaviour"); }
        void Update(Camera3D& /*cam*/, float /*dt*/) override { ++(*mCounter); }
    };

    CameraRegistry3D reg;
    reg.Register(Dia::Core::StringCRC("cam1"), Camera3D{});
    reg.Register(Dia::Core::StringCRC("cam2"), Camera3D{});
    reg.AttachBehaviour(Dia::Core::StringCRC("cam1"), new CountingBehaviour(&count1));
    reg.AttachBehaviour(Dia::Core::StringCRC("cam2"), new CountingBehaviour(&count2));

    reg.UpdateAll(0.016f);
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Registry, MaxCapacity_FillsAndHoldsAll)
{
    CameraRegistry3D reg;
    char name[8];
    for (unsigned int i = 0; i < CameraRegistry3D::kMaxCameras; ++i)
    {
        name[0] = 'a' + static_cast<char>(i); name[1] = '\0';
        EXPECT_TRUE(reg.Register(Dia::Core::StringCRC(name), Camera3D{}));
    }
    EXPECT_EQ(reg.GetCount(), CameraRegistry3D::kMaxCameras);
}

// ---------------------------------------------------------------------------
// CameraBuilder3D
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Registry, Builder_WithCamera_AsActive_Registers)
{
    Dia::Camera3D::Testing::CameraBuilder3D builder;
    builder.WithCamera("main", 0.0f, 0.0f, 10.0f).AsActive();

    CameraRegistry3D& reg = builder.Registry();
    EXPECT_TRUE(reg.Has(Dia::Core::StringCRC("main")));
    EXPECT_FLOAT_EQ(reg.GetActive().position.z, 10.0f);
}

TEST(DiaCamera3D_Registry, Builder_Perspective_SetsProjection)
{
    Dia::Camera3D::Testing::CameraBuilder3D builder;
    builder.WithCamera("cam").Perspective(45.0f, 0.1f, 500.0f);

    CameraRegistry3D& reg = builder.Registry();
    const Camera3D& cam = reg.Get(Dia::Core::StringCRC("cam"));
    EXPECT_EQ(cam.projectionType, ProjectionType::Perspective);
    EXPECT_FLOAT_EQ(cam.perspective.fovY,  45.0f);
    EXPECT_FLOAT_EQ(cam.perspective.nearZ,  0.1f);
    EXPECT_FLOAT_EQ(cam.perspective.farZ,  500.0f);
}

TEST(DiaCamera3D_Registry, Builder_Orthographic_SetsProjection)
{
    Dia::Camera3D::Testing::CameraBuilder3D builder;
    builder.WithCamera("ortho").Orthographic(20.0f, 15.0f, -50.0f, 50.0f);

    CameraRegistry3D& reg = builder.Registry();
    const Camera3D& cam = reg.Get(Dia::Core::StringCRC("ortho"));
    EXPECT_EQ(cam.projectionType, ProjectionType::Orthographic);
    EXPECT_FLOAT_EQ(cam.ortho.width,   20.0f);
    EXPECT_FLOAT_EQ(cam.ortho.height,  15.0f);
    EXPECT_FLOAT_EQ(cam.ortho.nearZ,  -50.0f);
    EXPECT_FLOAT_EQ(cam.ortho.farZ,    50.0f);
}
