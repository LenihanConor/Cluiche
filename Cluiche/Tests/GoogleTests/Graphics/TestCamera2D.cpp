////////////////////////////////////////////////////////////////////////////////
// TestCamera2D.cpp
// Tests for Camera2D and ViewportTransform — DiaGraphics module
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaGraphics/Camera/Camera2D.h>
#include <DiaGraphics/Camera/ViewportTransform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGeometry2D/Shapes/AARect.h>

using namespace Dia::Graphics;
using namespace Dia::Maths;

// ============================================================================
// Camera2D — construction suite
// ============================================================================

TEST(Camera2D_Construct, DefaultConstruct)
{
    Camera2D cam;
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(),       1.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(),   0.0f);
}

TEST(Camera2D_Construct, ExplicitConstruct)
{
    Camera2D cam(Vector2D(10.0f, 20.0f), 2.5f, 45.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 20.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(),        2.5f);
    EXPECT_FLOAT_EQ(cam.GetRotation(),   45.0f);
}

TEST(Camera2D_Construct, ExplicitConstruct_DefaultZoomAndRotation)
{
    Camera2D cam(Vector2D(5.0f, -3.0f));
    EXPECT_FLOAT_EQ(cam.GetZoom(),     1.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(), 0.0f);
}

// ============================================================================
// Camera2D — setters suite
// ============================================================================

TEST(Camera2D_Setters, SetPosition_RoundTrip)
{
    Camera2D cam;
    cam.SetPosition(Vector2D(7.0f, -4.0f));
    EXPECT_FLOAT_EQ(cam.GetPosition().x,  7.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, -4.0f);
}

TEST(Camera2D_Setters, SetZoom_RoundTrip)
{
    Camera2D cam;
    cam.SetZoom(3.14f);
    EXPECT_FLOAT_EQ(cam.GetZoom(), 3.14f);
}

TEST(Camera2D_Setters, SetRotation_RoundTrip)
{
    Camera2D cam;
    cam.SetRotation(90.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(), 90.0f);
}

TEST(Camera2D_Setters, Setters_AllThree_Independent)
{
    Camera2D cam;
    cam.SetPosition(Vector2D(1.0f, 2.0f));
    cam.SetZoom(4.0f);
    cam.SetRotation(180.0f);

    EXPECT_FLOAT_EQ(cam.GetPosition().x,   1.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y,   2.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(),         4.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(),   180.0f);
}

// ============================================================================
// ViewportTransform — helpers
// ============================================================================

namespace
{
    // Tolerance used throughout for EXPECT_NEAR comparisons.
    static const float kTol = 0.001f;

    // A common 800x600 window used as the default fixture size.
    static const Vector2D kWindow(800.0f, 600.0f);
}

// ============================================================================
// ViewportTransform — no rotation, zoom = 1 suite
// ============================================================================

TEST(ViewportTransform_NoRotZoom1, OriginMapsToScreenCenter)
{
    Camera2D cam;  // position (0,0), zoom 1, rotation 0
    ViewportTransform vt(cam, kWindow);

    Vector2D screen = vt.WorldToScreen(Vector2D(0.0f, 0.0f));
    EXPECT_NEAR(screen.x, kWindow.x * 0.5f, kTol);
    EXPECT_NEAR(screen.y, kWindow.y * 0.5f, kTol);
}

TEST(ViewportTransform_NoRotZoom1, ScreenCenterMapsToOrigin)
{
    Camera2D cam;
    ViewportTransform vt(cam, kWindow);

    Vector2D world = vt.ScreenToWorld(Vector2D(kWindow.x * 0.5f, kWindow.y * 0.5f));
    EXPECT_NEAR(world.x, 0.0f, kTol);
    EXPECT_NEAR(world.y, 0.0f, kTol);
}

TEST(ViewportTransform_NoRotZoom1, RoundTrip_WorldToScreen_Back)
{
    Camera2D cam;
    ViewportTransform vt(cam, kWindow);

    const Vector2D original(37.5f, -88.0f);
    Vector2D result = vt.ScreenToWorld(vt.WorldToScreen(original));
    EXPECT_NEAR(result.x, original.x, kTol);
    EXPECT_NEAR(result.y, original.y, kTol);
}

TEST(ViewportTransform_NoRotZoom1, RoundTrip_ScreenToWorld_Back)
{
    Camera2D cam;
    ViewportTransform vt(cam, kWindow);

    const Vector2D pixel(320.0f, 100.0f);
    Vector2D result = vt.WorldToScreen(vt.ScreenToWorld(pixel));
    EXPECT_NEAR(result.x, pixel.x, kTol);
    EXPECT_NEAR(result.y, pixel.y, kTol);
}

// ============================================================================
// ViewportTransform — zoom suite
// ============================================================================

TEST(ViewportTransform_Zoom, Zoom2_WorldToScreen_CloserToCenter)
{
    const Vector2D window(800.0f, 600.0f);
    const Vector2D center(window.x * 0.5f, window.y * 0.5f);
    const Vector2D worldPt(10.0f, 0.0f);

    Camera2D cam1;  // zoom = 1
    ViewportTransform vt1(cam1, window);
    Vector2D screen1 = vt1.WorldToScreen(worldPt);

    Camera2D cam2;  // zoom = 2
    cam2.SetZoom(2.0f);
    ViewportTransform vt2(cam2, window);
    Vector2D screen2 = vt2.WorldToScreen(worldPt);

    // At zoom=2 the point is twice as far from centre as at zoom=1.
    // Both are to the right of centre so distance is simply screen.x - center.x.
    float dist1 = screen1.x - center.x;
    float dist2 = screen2.x - center.x;
    EXPECT_NEAR(dist2, dist1 * 2.0f, kTol);
}

TEST(ViewportTransform_Zoom, Zoom2_ScreenToWorld_HalvesPixelOffset)
{
    const Vector2D window(800.0f, 600.0f);
    const Vector2D center(window.x * 0.5f, window.y * 0.5f);

    // A pixel 50 units to the right of centre.
    const Vector2D pixel(center.x + 50.0f, center.y);

    Camera2D cam;
    cam.SetZoom(2.0f);
    ViewportTransform vt(cam, window);

    Vector2D world = vt.ScreenToWorld(pixel);
    // At zoom=2 each screen pixel represents 0.5 world units, so offset = 25
    EXPECT_NEAR(world.x, 25.0f, kTol);
    EXPECT_NEAR(world.y,  0.0f, kTol);
}

// ============================================================================
// ViewportTransform — camera offset suite
// ============================================================================

TEST(ViewportTransform_CameraOffset, CameraAt100_WorldPoint100_MapsToCenter)
{
    const Vector2D window(800.0f, 600.0f);

    Camera2D cam(Vector2D(100.0f, 0.0f));
    ViewportTransform vt(cam, window);

    Vector2D screen = vt.WorldToScreen(Vector2D(100.0f, 0.0f));
    EXPECT_NEAR(screen.x, window.x * 0.5f, kTol);
    EXPECT_NEAR(screen.y, window.y * 0.5f, kTol);
}

TEST(ViewportTransform_CameraOffset, ScreenCenterMaps_ToCameraPosition)
{
    const Vector2D window(800.0f, 600.0f);
    const Vector2D camPos(100.0f, 50.0f);

    Camera2D cam(camPos);
    ViewportTransform vt(cam, window);

    Vector2D world = vt.ScreenToWorld(Vector2D(window.x * 0.5f, window.y * 0.5f));
    EXPECT_NEAR(world.x, camPos.x, kTol);
    EXPECT_NEAR(world.y, camPos.y, kTol);
}

// ============================================================================
// ViewportTransform::GetWorldBounds suite
// ============================================================================

TEST(ViewportTransform_WorldBounds, DefaultCamera_BoundsContainOrigin)
{
    Camera2D cam;
    ViewportTransform vt(cam, kWindow);

    Dia::Geometry2D::AARect bounds = vt.GetWorldBounds();

    // Origin (0,0) must be inside the visible world bounds.
    // AARect::IsIntersecting returns a non-zero (intersecting) result for a
    // point that lies within or on the boundary.
    Dia::Geometry2D::IntersectionClassify result = bounds.IsIntersecting(Vector2D(0.0f, 0.0f));
    EXPECT_TRUE(result.IsIntersecting());
}

TEST(ViewportTransform_WorldBounds, CameraOffset_BoundsCenter_NearCameraPosition)
{
    const Vector2D camPos(200.0f, 150.0f);
    Camera2D cam(camPos);
    ViewportTransform vt(cam, kWindow);

    Dia::Geometry2D::AARect bounds = vt.GetWorldBounds();
    Vector2D center = bounds.CalculateCenter();

    EXPECT_NEAR(center.x, camPos.x, kTol);
    EXPECT_NEAR(center.y, camPos.y, kTol);
}

TEST(ViewportTransform_WorldBounds, DefaultCamera_BoundsSpanCorrectSize)
{
    // At zoom=1 the world bounds width should equal the window width,
    // and height should equal the window height.
    Camera2D cam;
    ViewportTransform vt(cam, kWindow);

    Dia::Geometry2D::AARect bounds = vt.GetWorldBounds();
    float width  = bounds.GetTopRight().x - bounds.GetBottomLeft().x;
    float height = bounds.GetTopRight().y - bounds.GetBottomLeft().y;

    EXPECT_NEAR(width,  kWindow.x, kTol);
    EXPECT_NEAR(height, kWindow.y, kTol);
}

TEST(ViewportTransform_WorldBounds, Zoom2_BoundsHalfWindowSize)
{
    // At zoom=2 the visible world area shrinks to half width and half height.
    Camera2D cam;
    cam.SetZoom(2.0f);
    ViewportTransform vt(cam, kWindow);

    Dia::Geometry2D::AARect bounds = vt.GetWorldBounds();
    float width  = bounds.GetTopRight().x - bounds.GetBottomLeft().x;
    float height = bounds.GetTopRight().y - bounds.GetBottomLeft().y;

    EXPECT_NEAR(width,  kWindow.x * 0.5f, kTol);
    EXPECT_NEAR(height, kWindow.y * 0.5f, kTol);
}

// ============================================================================
// ViewportTransform — rotation suite
// ============================================================================

#include <DiaGraphics/Frame/FrameData.h>
#include <cmath>

TEST(ViewportTransform_Rotation, Rotate90_XAxisBecomesY)
{
    Camera2D cam;
    cam.SetRotation(90.0f);
    ViewportTransform vt(cam, kWindow);

    const Vector2D center(kWindow.x * 0.5f, kWindow.y * 0.5f);
    Vector2D screen = vt.WorldToScreen(Vector2D(10.0f, 0.0f));

    float dx = screen.x - center.x;
    float dy = screen.y - center.y;

    EXPECT_NEAR(dx, 0.0f, kTol);
    EXPECT_NEAR(std::abs(dy), 10.0f, kTol);
}

TEST(ViewportTransform_Rotation, RoundTrip_45Degrees)
{
    Camera2D cam;
    cam.SetRotation(45.0f);
    ViewportTransform vt(cam, kWindow);

    const Vector2D original(37.5f, -88.0f);
    Vector2D result = vt.ScreenToWorld(vt.WorldToScreen(original));

    EXPECT_NEAR(result.x, original.x, kTol);
    EXPECT_NEAR(result.y, original.y, kTol);
}

// ============================================================================
// ViewportTransform — edge case suite
// ============================================================================

TEST(ViewportTransform_EdgeCases, ZoomNearZero_FiniteResults)
{
    Camera2D cam;
    cam.SetZoom(0.001f);
    ViewportTransform vt(cam, kWindow);

    Vector2D screen = vt.WorldToScreen(Vector2D(1.0f, 1.0f));
    EXPECT_TRUE(std::isfinite(screen.x));
    EXPECT_TRUE(std::isfinite(screen.y));

    Vector2D world = vt.ScreenToWorld(Vector2D(400.0f, 300.0f));
    EXPECT_TRUE(std::isfinite(world.x));
    EXPECT_TRUE(std::isfinite(world.y));
}

TEST(ViewportTransform_EdgeCases, LargeWorldCoordinates)
{
    Camera2D cam;
    ViewportTransform vt(cam, kWindow);

    Vector2D screen = vt.WorldToScreen(Vector2D(100000.0f, 100000.0f));
    EXPECT_TRUE(std::isfinite(screen.x));
    EXPECT_TRUE(std::isfinite(screen.y));
}

TEST(ViewportTransform_EdgeCases, NegativeWorldCoordinates)
{
    Camera2D cam;
    ViewportTransform vt(cam, kWindow);

    Vector2D screen = vt.WorldToScreen(Vector2D(-500.0f, -300.0f));
    EXPECT_TRUE(std::isfinite(screen.x));
    EXPECT_TRUE(std::isfinite(screen.y));
}

TEST(ViewportTransform_EdgeCases, WindowSizeOne)
{
    Camera2D cam;
    const Vector2D tinyWindow(1.0f, 1.0f);
    ViewportTransform vt(cam, tinyWindow);

    Dia::Geometry2D::AARect bounds = vt.GetWorldBounds();
    float width  = bounds.GetTopRight().x - bounds.GetBottomLeft().x;
    float height = bounds.GetTopRight().y - bounds.GetBottomLeft().y;

    EXPECT_NEAR(width,  1.0f, kTol);
    EXPECT_NEAR(height, 1.0f, kTol);
}

// ============================================================================
// FrameData — camera integration suite
// ============================================================================

TEST(FrameData_Camera, DefaultCamera_IsIdentity)
{
    Dia::Graphics::FrameData fd;
    const Camera2D& cam = fd.GetCamera();

    EXPECT_FLOAT_EQ(cam.GetPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(),       1.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(),   0.0f);
}

TEST(FrameData_Camera, SetGet_Camera_RoundTrip)
{
    Dia::Graphics::FrameData fd;
    Camera2D cam(Vector2D(42.0f, -7.0f), 3.0f, 90.0f);
    fd.SetCamera(cam);

    const Camera2D& got = fd.GetCamera();
    EXPECT_FLOAT_EQ(got.GetPosition().x, 42.0f);
    EXPECT_FLOAT_EQ(got.GetPosition().y, -7.0f);
    EXPECT_FLOAT_EQ(got.GetZoom(),        3.0f);
    EXPECT_FLOAT_EQ(got.GetRotation(),   90.0f);
}

TEST(FrameData_Camera, SetGet_MousePixel_RoundTrip)
{
    Dia::Graphics::FrameData fd;
    const Vector2D pixel(123.0f, 456.0f);
    fd.SetMousePixel(pixel);

    EXPECT_FLOAT_EQ(fd.GetMousePixel().x, 123.0f);
    EXPECT_FLOAT_EQ(fd.GetMousePixel().y, 456.0f);
}

TEST(FrameData_Camera, Clear_ResetsCamera)
{
    Dia::Graphics::FrameData fd;
    fd.SetCamera(Camera2D(Vector2D(100.0f, 200.0f), 5.0f, 45.0f));
    fd.SetMousePixel(Vector2D(320.0f, 240.0f));
    fd.Clear();

    const Camera2D& cam = fd.GetCamera();
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 0.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(),       1.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(),   0.0f);
    EXPECT_FLOAT_EQ(fd.GetMousePixel().x, 0.0f);
    EXPECT_FLOAT_EQ(fd.GetMousePixel().y, 0.0f);
}

TEST(FrameData_Camera, Copy_PreservesCamera)
{
    Dia::Graphics::FrameData src;
    src.SetCamera(Camera2D(Vector2D(10.0f, 20.0f), 2.0f, 30.0f));
    src.SetMousePixel(Vector2D(111.0f, 222.0f));

    Dia::Graphics::FrameData dst;
    dst.Copy(src);

    const Camera2D& cam = dst.GetCamera();
    EXPECT_FLOAT_EQ(cam.GetPosition().x, 10.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y, 20.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(),        2.0f);
    EXPECT_FLOAT_EQ(cam.GetRotation(),   30.0f);
    EXPECT_FLOAT_EQ(dst.GetMousePixel().x, 111.0f);
    EXPECT_FLOAT_EQ(dst.GetMousePixel().y, 222.0f);
}

TEST(FrameData_Camera, Assign_PreservesCamera)
{
    Dia::Graphics::FrameData src;
    src.SetCamera(Camera2D(Vector2D(-5.0f, 8.0f), 0.5f, 180.0f));
    src.SetMousePixel(Vector2D(640.0f, 480.0f));

    Dia::Graphics::FrameData dst;
    dst = src;

    const Camera2D& cam = dst.GetCamera();
    EXPECT_FLOAT_EQ(cam.GetPosition().x,  -5.0f);
    EXPECT_FLOAT_EQ(cam.GetPosition().y,   8.0f);
    EXPECT_FLOAT_EQ(cam.GetZoom(),         0.5f);
    EXPECT_FLOAT_EQ(cam.GetRotation(),   180.0f);
    EXPECT_FLOAT_EQ(dst.GetMousePixel().x, 640.0f);
    EXPECT_FLOAT_EQ(dst.GetMousePixel().y, 480.0f);
}
