#include <gtest/gtest.h>

#include <DiaCamera3D/Registry/CameraBehaviourRegistry3D.h>
#include <DiaCamera3D/Behaviours/Follow3D.h>
#include <DiaCamera3D/Behaviours/SmoothDamp3D.h>
#include <DiaCamera3D/Behaviours/BoundsClamp3D.h>
#include <DiaCamera3D/Behaviours/ScreenShake3D.h>
#include <DiaCamera3D/Behaviours/Orbit.h>
#include <DiaCamera3D/Behaviours/Flythrough.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Camera3D;

// ---------------------------------------------------------------------------
// Self-registration checks — each behaviour registers on first TU inclusion
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_BehaviourRegistry, Follow3D_IsRegistered)
{
    EXPECT_TRUE(CameraBehaviourRegistry3D::Get().IsRegistered(
        Dia::Core::StringCRC(Follow3D::kTypeIdStr)));
}

TEST(DiaCamera3D_BehaviourRegistry, SmoothDamp3D_IsRegistered)
{
    EXPECT_TRUE(CameraBehaviourRegistry3D::Get().IsRegistered(
        Dia::Core::StringCRC(SmoothDamp3D::kTypeIdStr)));
}

TEST(DiaCamera3D_BehaviourRegistry, BoundsClamp3D_IsRegistered)
{
    EXPECT_TRUE(CameraBehaviourRegistry3D::Get().IsRegistered(
        Dia::Core::StringCRC(BoundsClamp3D::kTypeIdStr)));
}

TEST(DiaCamera3D_BehaviourRegistry, ScreenShake3D_IsRegistered)
{
    EXPECT_TRUE(CameraBehaviourRegistry3D::Get().IsRegistered(
        Dia::Core::StringCRC(ScreenShake3D::kTypeIdStr)));
}

TEST(DiaCamera3D_BehaviourRegistry, Orbit_IsRegistered)
{
    EXPECT_TRUE(CameraBehaviourRegistry3D::Get().IsRegistered(
        Dia::Core::StringCRC(Orbit::kTypeIdStr)));
}

TEST(DiaCamera3D_BehaviourRegistry, Flythrough_IsRegistered)
{
    EXPECT_TRUE(CameraBehaviourRegistry3D::Get().IsRegistered(
        Dia::Core::StringCRC(Flythrough::kTypeIdStr)));
}

TEST(DiaCamera3D_BehaviourRegistry, Unknown_IsNotRegistered)
{
    EXPECT_FALSE(CameraBehaviourRegistry3D::Get().IsRegistered(
        Dia::Core::StringCRC("nonexistent_behaviour_xyz")));
}

// ---------------------------------------------------------------------------
// Factory creation
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_BehaviourRegistry, Create_Follow3D_ReturnsNonNull)
{
    ICameraBehaviour3D* b = CameraBehaviourRegistry3D::Get().Create(
        Dia::Core::StringCRC(Follow3D::kTypeIdStr));
    ASSERT_NE(b, nullptr);
    delete b;
}

TEST(DiaCamera3D_BehaviourRegistry, Create_Unknown_ReturnsNull)
{
    ICameraBehaviour3D* b = CameraBehaviourRegistry3D::Get().Create(
        Dia::Core::StringCRC("no_such_behaviour_abc"));
    EXPECT_EQ(b, nullptr);
}

// Create all 6 engine behaviour types via factory
TEST(DiaCamera3D_BehaviourRegistry, Create_SmoothDamp3D_ReturnsNonNull)
{
    ICameraBehaviour3D* b = CameraBehaviourRegistry3D::Get().Create(
        Dia::Core::StringCRC(SmoothDamp3D::kTypeIdStr));
    ASSERT_NE(b, nullptr);
    delete b;
}

TEST(DiaCamera3D_BehaviourRegistry, Create_BoundsClamp3D_ReturnsNonNull)
{
    ICameraBehaviour3D* b = CameraBehaviourRegistry3D::Get().Create(
        Dia::Core::StringCRC(BoundsClamp3D::kTypeIdStr));
    ASSERT_NE(b, nullptr);
    delete b;
}

TEST(DiaCamera3D_BehaviourRegistry, Create_ScreenShake3D_ReturnsNonNull)
{
    ICameraBehaviour3D* b = CameraBehaviourRegistry3D::Get().Create(
        Dia::Core::StringCRC(ScreenShake3D::kTypeIdStr));
    ASSERT_NE(b, nullptr);
    delete b;
}

TEST(DiaCamera3D_BehaviourRegistry, Create_Orbit_ReturnsNonNull)
{
    ICameraBehaviour3D* b = CameraBehaviourRegistry3D::Get().Create(
        Dia::Core::StringCRC(Orbit::kTypeIdStr));
    ASSERT_NE(b, nullptr);
    delete b;
}

TEST(DiaCamera3D_BehaviourRegistry, Create_Flythrough_ReturnsNonNull)
{
    ICameraBehaviour3D* b = CameraBehaviourRegistry3D::Get().Create(
        Dia::Core::StringCRC(Flythrough::kTypeIdStr));
    ASSERT_NE(b, nullptr);
    delete b;
}
