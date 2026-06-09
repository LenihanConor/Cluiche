#include <gtest/gtest.h>

#include <DiaCamera3D/Camera3D.h>
#include <DiaCamera3D/ViewportTransform3D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::Camera3D;

// ---------------------------------------------------------------------------
// Camera3D defaults
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_Camera3D, DefaultConstruct_PerspectiveDefaults)
{
    Camera3D cam;
    EXPECT_EQ(cam.projectionType, ProjectionType::Perspective);
    EXPECT_FLOAT_EQ(cam.perspective.fovY,   60.0f);
    EXPECT_FLOAT_EQ(cam.perspective.nearZ,   0.1f);
    EXPECT_FLOAT_EQ(cam.perspective.farZ, 1000.0f);
}

TEST(DiaCamera3D_Camera3D, DefaultConstruct_OrthoDefaults)
{
    Camera3D cam;
    EXPECT_FLOAT_EQ(cam.ortho.width,   10.0f);
    EXPECT_FLOAT_EQ(cam.ortho.height,  10.0f);
    EXPECT_FLOAT_EQ(cam.ortho.nearZ, -100.0f);
    EXPECT_FLOAT_EQ(cam.ortho.farZ,   100.0f);
}

TEST(DiaCamera3D_Camera3D, SetProjectionType_Orthographic_Roundtrips)
{
    Camera3D cam;
    cam.projectionType = ProjectionType::Orthographic;
    EXPECT_EQ(cam.projectionType, ProjectionType::Orthographic);
}

// ---------------------------------------------------------------------------
// ViewportTransform3D
// ---------------------------------------------------------------------------

TEST(DiaCamera3D_ViewportTransform3D, GetViewMatrix_ReturnsMatrix)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 10.0f);

    ViewportTransform3D vp(cam, Dia::Maths::Vector2D(800.0f, 600.0f));
    const Dia::Maths::Matrix44& view = vp.GetViewMatrix();

    // A non-trivial view matrix will not be all zeros.
    bool anyNonZero = false;
    for (int r = 0; r < 4 && !anyNonZero; ++r)
        for (int c = 0; c < 4 && !anyNonZero; ++c)
            if (view.m[r][c] != 0.0f)
                anyNonZero = true;
    EXPECT_TRUE(anyNonZero);
}

TEST(DiaCamera3D_ViewportTransform3D, GetProjectionMatrix_ReturnsMatrix)
{
    Camera3D cam;
    ViewportTransform3D vp(cam, Dia::Maths::Vector2D(800.0f, 600.0f));
    const Dia::Maths::Matrix44& proj = vp.GetProjectionMatrix();

    bool anyNonZero = false;
    for (int r = 0; r < 4 && !anyNonZero; ++r)
        for (int c = 0; c < 4 && !anyNonZero; ++c)
            if (proj.m[r][c] != 0.0f)
                anyNonZero = true;
    EXPECT_TRUE(anyNonZero);
}

TEST(DiaCamera3D_ViewportTransform3D, WorldToScreen_CentrePoint_ReturnsTrue)
{
    // Camera at (0, 0, 10) looking toward -Z. Origin is directly in front.
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 10.0f);
    // Identity orientation → looking down -Z

    const Dia::Maths::Vector2D windowSize(800.0f, 600.0f);
    ViewportTransform3D vp(cam, windowSize);

    Dia::Maths::Vector2D screen;
    const bool result = vp.WorldToScreen(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f), screen);
    EXPECT_TRUE(result);
    // Should be close to screen centre (tolerance of ±5 pixels)
    EXPECT_NEAR(screen.x, windowSize.x * 0.5f, 5.0f);
    EXPECT_NEAR(screen.y, windowSize.y * 0.5f, 5.0f);
}

TEST(DiaCamera3D_ViewportTransform3D, WorldToScreen_BehindCamera_ReturnsFalse)
{
    // Camera at origin, identity orientation → looking down -Z.
    // A point at (0, 0, 5) is in the +Z direction — behind the camera.
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);

    ViewportTransform3D vp(cam, Dia::Maths::Vector2D(800.0f, 600.0f));

    Dia::Maths::Vector2D screen;
    const bool result = vp.WorldToScreen(Dia::Maths::Vector3D(0.0f, 0.0f, 5.0f), screen);
    EXPECT_FALSE(result);
}

TEST(DiaCamera3D_ViewportTransform3D, ScreenToWorldRay_DirectionIsNormalised)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 10.0f);
    const Dia::Maths::Vector2D windowSize(800.0f, 600.0f);

    ViewportTransform3D vp(cam, windowSize);

    Dia::Maths::Vector3D origin, dir;
    vp.ScreenToWorldRay(Dia::Maths::Vector2D(400.0f, 300.0f), origin, dir);

    const float mag = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    EXPECT_NEAR(mag, 1.0f, 0.001f);
}

TEST(DiaCamera3D_ViewportTransform3D, ExtractFrustum_DoesNotCrash)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 5.0f);
    ViewportTransform3D vp(cam, Dia::Maths::Vector2D(1280.0f, 720.0f));

    // Should not crash or assert
    const Dia::Geometry3D::Frustum frustum = vp.ExtractFrustum();
    (void)frustum;  // suppress unused-variable warning
}

TEST(DiaCamera3D_ViewportTransform3D, OrthoCamera_WorldToScreen_ReturnsTrue)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 10.0f);
    cam.projectionType = ProjectionType::Orthographic;
    cam.ortho.width  = 20.0f;
    cam.ortho.height = 15.0f;
    cam.ortho.nearZ  = 0.1f;
    cam.ortho.farZ   = 100.0f;

    ViewportTransform3D vp(cam, Dia::Maths::Vector2D(800.0f, 600.0f));

    Dia::Maths::Vector2D screen;
    // Origin is in front of the ortho camera (camera at z=10, looking -Z, origin at z=0)
    const bool result = vp.WorldToScreen(Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f), screen);
    EXPECT_TRUE(result);
}

TEST(DiaCamera3D_ViewportTransform3D, WorldToScreen_ScreenToWorldRay_RoundTrip)
{
    // Project a world point to screen, then cast a ray from that pixel.
    // The ray must pass through (or very close to) the original world point.
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 10.0f);
    // Identity orientation → looking down -Z

    const Dia::Maths::Vector2D windowSize(800.0f, 600.0f);
    ViewportTransform3D vp(cam, windowSize);

    const Dia::Maths::Vector3D worldPt(1.0f, 2.0f, -5.0f);

    Dia::Maths::Vector2D screenPt;
    ASSERT_TRUE(vp.WorldToScreen(worldPt, screenPt));

    Dia::Maths::Vector3D origin, dir;
    vp.ScreenToWorldRay(screenPt, origin, dir);

    // The ray origin is the near plane. The vector from origin to worldPt
    // must be parallel to dir (within tolerance).
    const Dia::Maths::Vector3D toTarget(
        worldPt.x - origin.x,
        worldPt.y - origin.y,
        worldPt.z - origin.z);

    // Dot product of normalised toTarget with dir must be ~1 (parallel)
    const float len = std::sqrt(toTarget.x*toTarget.x + toTarget.y*toTarget.y + toTarget.z*toTarget.z);
    ASSERT_GT(len, 0.001f);
    const float dot = (toTarget.x/len)*dir.x + (toTarget.y/len)*dir.y + (toTarget.z/len)*dir.z;
    EXPECT_NEAR(dot, 1.0f, 0.01f);
}

TEST(DiaCamera3D_ViewportTransform3D, ScreenToWorldRay_TopLeftCorner_IsNormalised)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 5.0f);
    const Dia::Maths::Vector2D windowSize(800.0f, 600.0f);
    ViewportTransform3D vp(cam, windowSize);

    Dia::Maths::Vector3D origin, dir;
    vp.ScreenToWorldRay(Dia::Maths::Vector2D(0.0f, 0.0f), origin, dir);

    const float mag = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    EXPECT_NEAR(mag, 1.0f, 0.001f);
}

TEST(DiaCamera3D_ViewportTransform3D, ScreenToWorldRay_BottomRightCorner_IsNormalised)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 5.0f);
    const Dia::Maths::Vector2D windowSize(800.0f, 600.0f);
    ViewportTransform3D vp(cam, windowSize);

    Dia::Maths::Vector3D origin, dir;
    vp.ScreenToWorldRay(Dia::Maths::Vector2D(800.0f, 600.0f), origin, dir);

    const float mag = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    EXPECT_NEAR(mag, 1.0f, 0.001f);
}

TEST(DiaCamera3D_ViewportTransform3D, DifferentAspectRatio_DifferentProjectionMatrix)
{
    Camera3D cam;
    ViewportTransform3D vp169(cam, Dia::Maths::Vector2D(1920.0f, 1080.0f));
    ViewportTransform3D vp43 (cam, Dia::Maths::Vector2D(800.0f,  600.0f));

    // At least one matrix element must differ between the two aspect ratios
    const Dia::Maths::Matrix44& p169 = vp169.GetProjectionMatrix();
    const Dia::Maths::Matrix44& p43  = vp43.GetProjectionMatrix();
    bool differs = false;
    for (int r = 0; r < 4 && !differs; ++r)
        for (int c = 0; c < 4 && !differs; ++c)
            if (std::abs(p169.m[r][c] - p43.m[r][c]) > 0.0001f)
                differs = true;
    EXPECT_TRUE(differs);
}

TEST(DiaCamera3D_ViewportTransform3D, OrthoCamera_ScreenToWorldRay_IsNormalised)
{
    Camera3D cam;
    cam.position = Dia::Maths::Vector3D(0.0f, 0.0f, 10.0f);
    cam.projectionType = ProjectionType::Orthographic;
    cam.ortho.width  = 20.0f;
    cam.ortho.height = 15.0f;
    cam.ortho.nearZ  = 0.1f;
    cam.ortho.farZ   = 100.0f;

    ViewportTransform3D vp(cam, Dia::Maths::Vector2D(800.0f, 600.0f));

    Dia::Maths::Vector3D origin, dir;
    vp.ScreenToWorldRay(Dia::Maths::Vector2D(400.0f, 300.0f), origin, dir);

    const float mag = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    EXPECT_NEAR(mag, 1.0f, 0.001f);
}
