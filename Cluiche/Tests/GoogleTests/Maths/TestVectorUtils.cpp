// TestVectorUtils.cpp - Google Test unit tests for VectorUtils
//
// Tests the Dia::Maths::VectorUtils namespace for vector conversions and swizzling

#include <gtest/gtest.h>
#include <DiaMaths/Vector/VectorUtils.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Vector/Vector4D.h>

using namespace Dia::Maths;
using namespace Dia::Maths::VectorUtils;

constexpr float kEpsilon = 0.0001f;

// ==============================================================================
// Vector2D from Vector3D Conversion Tests
// ==============================================================================

TEST(VectorUtils, ToVector2DFromVector3D_CopiesXY)
{
    Vector3D v3(1.0f, 2.0f, 3.0f);
    Vector2D v2;

    ToVector2DFromVector3D(v2, v3);

    EXPECT_FLOAT_EQ(v2.x, 1.0f);
    EXPECT_FLOAT_EQ(v2.y, 2.0f);
}

TEST(VectorUtils, Vector2DFromVector3DXY_CopiesXYComponents)
{
    Vector3D v3(10.0f, 20.0f, 30.0f);
    Vector2D v2;

    Vector2DFromVector3DXY(v2, v3);

    EXPECT_FLOAT_EQ(v2.x, 10.0f);
    EXPECT_FLOAT_EQ(v2.y, 20.0f);
}

TEST(VectorUtils, Vector2DFromVector3DXZ_CopiesXZComponents)
{
    Vector3D v3(10.0f, 20.0f, 30.0f);
    Vector2D v2;

    Vector2DFromVector3DXZ(v2, v3);

    EXPECT_FLOAT_EQ(v2.x, 10.0f);
    EXPECT_FLOAT_EQ(v2.y, 30.0f);  // Z mapped to Y
}

TEST(VectorUtils, Vector2DFromVector3DZY_CopiesZYComponents)
{
    Vector3D v3(10.0f, 20.0f, 30.0f);
    Vector2D v2;

    Vector2DFromVector3DZY(v2, v3);

    EXPECT_FLOAT_EQ(v2.x, 30.0f);  // Z mapped to X
    EXPECT_FLOAT_EQ(v2.y, 20.0f);
}

// ==============================================================================
// Vector2D from Vector4D Conversion Tests
// ==============================================================================

TEST(VectorUtils, ToVector2DFromVector4D_CopiesXY)
{
    Vector4D v4(1.0f, 2.0f, 3.0f, 4.0f);
    Vector2D v2;

    ToVector2DFromVector4D(v2, v4);

    EXPECT_FLOAT_EQ(v2.x, 1.0f);
    EXPECT_FLOAT_EQ(v2.y, 2.0f);
}

TEST(VectorUtils, Vector2DFromVector4DXY_CopiesXYComponents)
{
    Vector4D v4(10.0f, 20.0f, 30.0f, 40.0f);
    Vector2D v2;

    Vector2DFromVector4DXY(v2, v4);

    EXPECT_FLOAT_EQ(v2.x, 10.0f);
    EXPECT_FLOAT_EQ(v2.y, 20.0f);
}

TEST(VectorUtils, Vector2DFromVector4DXZ_CopiesXZComponents)
{
    Vector4D v4(10.0f, 20.0f, 30.0f, 40.0f);
    Vector2D v2;

    Vector2DFromVector4DXZ(v2, v4);

    EXPECT_FLOAT_EQ(v2.x, 10.0f);
    EXPECT_FLOAT_EQ(v2.y, 30.0f);
}

TEST(VectorUtils, Vector2DFromVector4DZY_CopiesZYComponents)
{
    Vector4D v4(10.0f, 20.0f, 30.0f, 40.0f);
    Vector2D v2;

    Vector2DFromVector4DZY(v2, v4);

    EXPECT_FLOAT_EQ(v2.x, 30.0f);
    EXPECT_FLOAT_EQ(v2.y, 20.0f);
}

// ==============================================================================
// Vector3D from Vector2D Conversion Tests
// ==============================================================================

TEST(VectorUtils, ToVector3DFromVector2D_CopiesXYSetsZToZero)
{
    Vector2D v2(5.0f, 10.0f);
    Vector3D v3;

    ToVector3DFromVector2D(v3, v2);

    EXPECT_FLOAT_EQ(v3.x, 5.0f);
    EXPECT_FLOAT_EQ(v3.y, 10.0f);
    EXPECT_FLOAT_EQ(v3.z, 0.0f);
}

TEST(VectorUtils, Vector3DXYFromVector2D_MapsToXYPlane)
{
    Vector2D v2(7.0f, 8.0f);
    Vector3D v3;

    Vector3DXYFromVector2D(v3, v2);

    EXPECT_FLOAT_EQ(v3.x, 7.0f);
    EXPECT_FLOAT_EQ(v3.y, 8.0f);
    EXPECT_FLOAT_EQ(v3.z, 0.0f);
}

TEST(VectorUtils, Vector3DXZFromVector2D_MapsToXZPlane)
{
    Vector2D v2(7.0f, 8.0f);
    Vector3D v3;

    Vector3DXZFromVector2D(v3, v2);

    EXPECT_FLOAT_EQ(v3.x, 7.0f);
    EXPECT_FLOAT_EQ(v3.y, 0.0f);
    EXPECT_FLOAT_EQ(v3.z, 8.0f);  // Y mapped to Z
}

TEST(VectorUtils, Vector3DYZFromVector2D_MapsToYZPlane)
{
    Vector2D v2(7.0f, 8.0f);
    Vector3D v3;

    Vector3DYZFromVector2D(v3, v2);

    EXPECT_FLOAT_EQ(v3.x, 0.0f);
    EXPECT_FLOAT_EQ(v3.y, 7.0f);  // X mapped to Y
    EXPECT_FLOAT_EQ(v3.z, 8.0f);  // Y mapped to Z
}

// ==============================================================================
// Vector3D from Vector4D Conversion Tests
// ==============================================================================

TEST(VectorUtils, ToVector3DFromVector4D_CopiesXYZ)
{
    Vector4D v4(1.0f, 2.0f, 3.0f, 4.0f);
    Vector3D v3;

    ToVector3DFromVector4D(v3, v4);

    EXPECT_FLOAT_EQ(v3.x, 1.0f);
    EXPECT_FLOAT_EQ(v3.y, 2.0f);
    EXPECT_FLOAT_EQ(v3.z, 3.0f);
}

// ==============================================================================
// Vector4D from Vector2D Conversion Tests
// ==============================================================================

TEST(VectorUtils, ToVector4DPointFromVector2D_SetsWToOne)
{
    Vector2D v2(5.0f, 10.0f);
    Vector4D v4;

    ToVector4DPointFromVector2D(v4, v2);

    EXPECT_FLOAT_EQ(v4.x, 5.0f);
    EXPECT_FLOAT_EQ(v4.y, 10.0f);
    EXPECT_FLOAT_EQ(v4.z, 0.0f);
    EXPECT_FLOAT_EQ(v4.w, 1.0f);  // Points have w=1
}

TEST(VectorUtils, ToVector4DVectorFromVector2D_SetsWToZero)
{
    Vector2D v2(5.0f, 10.0f);
    Vector4D v4;

    ToVector4DVectorFromVector2D(v4, v2);

    EXPECT_FLOAT_EQ(v4.x, 5.0f);
    EXPECT_FLOAT_EQ(v4.y, 10.0f);
    EXPECT_FLOAT_EQ(v4.z, 0.0f);
    EXPECT_FLOAT_EQ(v4.w, 0.0f);  // Directions have w=0
}

TEST(VectorUtils, Vector4DXYFromVector2D_MapsToXYWithZeroZW)
{
    Vector2D v2(3.0f, 4.0f);
    Vector4D v4;

    Vector4DXYFromVector2D(v4, v2);

    EXPECT_FLOAT_EQ(v4.x, 3.0f);
    EXPECT_FLOAT_EQ(v4.y, 4.0f);
    EXPECT_FLOAT_EQ(v4.z, 0.0f);
    EXPECT_FLOAT_EQ(v4.w, 0.0f);
}

TEST(VectorUtils, Vector4DXZFromVector2D_MapsToXZWithZeroYW)
{
    Vector2D v2(3.0f, 4.0f);
    Vector4D v4;

    Vector4DXZFromVector2D(v4, v2);

    EXPECT_FLOAT_EQ(v4.x, 3.0f);
    EXPECT_FLOAT_EQ(v4.y, 0.0f);
    EXPECT_FLOAT_EQ(v4.z, 4.0f);
    EXPECT_FLOAT_EQ(v4.w, 0.0f);
}

TEST(VectorUtils, Vector4DYZFromVector2D_MapsToYZWithZeroXW)
{
    Vector2D v2(3.0f, 4.0f);
    Vector4D v4;

    Vector4DYZFromVector2D(v4, v2);

    EXPECT_FLOAT_EQ(v4.x, 0.0f);
    EXPECT_FLOAT_EQ(v4.y, 3.0f);
    EXPECT_FLOAT_EQ(v4.z, 4.0f);
    EXPECT_FLOAT_EQ(v4.w, 0.0f);
}

// ==============================================================================
// Vector4D from Vector3D Conversion Tests
// ==============================================================================

TEST(VectorUtils, ToVector4DPointFromVector3D_SetsWToOne)
{
    Vector3D v3(1.0f, 2.0f, 3.0f);
    Vector4D v4;

    ToVector4DPointFromVector3D(v4, v3);

    EXPECT_FLOAT_EQ(v4.x, 1.0f);
    EXPECT_FLOAT_EQ(v4.y, 2.0f);
    EXPECT_FLOAT_EQ(v4.z, 3.0f);
    EXPECT_FLOAT_EQ(v4.w, 1.0f);  // Points have w=1
}

TEST(VectorUtils, ToVector4DVectorFromVector3D_SetsWToZero)
{
    Vector3D v3(1.0f, 2.0f, 3.0f);
    Vector4D v4;

    ToVector4DVectorFromVector3D(v4, v3);

    EXPECT_FLOAT_EQ(v4.x, 1.0f);
    EXPECT_FLOAT_EQ(v4.y, 2.0f);
    EXPECT_FLOAT_EQ(v4.z, 3.0f);
    EXPECT_FLOAT_EQ(v4.w, 0.0f);  // Directions have w=0
}

// ==============================================================================
// Vector2D Swizzling Tests
// ==============================================================================

TEST(VectorUtils, Vector2DSwizzleYX_SwapsComponents)
{
    Vector2D xy(10.0f, 20.0f);
    Vector2D yx;

    Vector2DSwizzleYX(yx, xy);

    EXPECT_FLOAT_EQ(yx.x, 20.0f);  // Original Y
    EXPECT_FLOAT_EQ(yx.y, 10.0f);  // Original X
}

// ==============================================================================
// Vector3D Swizzling Tests
// ==============================================================================

TEST(VectorUtils, Vector3DSwizzleXZY_ReordersComponents)
{
    Vector3D xyz(1.0f, 2.0f, 3.0f);
    Vector3D xzy;

    Vector3DSwizzleXZY(xzy, xyz);

    EXPECT_FLOAT_EQ(xzy.x, 1.0f);  // X unchanged
    EXPECT_FLOAT_EQ(xzy.y, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(xzy.z, 2.0f);  // Original Y
}

TEST(VectorUtils, Vector3DSwizzleYXZ_ReordersComponents)
{
    Vector3D xyz(1.0f, 2.0f, 3.0f);
    Vector3D yxz;

    Vector3DSwizzleYXZ(yxz, xyz);

    EXPECT_FLOAT_EQ(yxz.x, 2.0f);  // Original Y
    EXPECT_FLOAT_EQ(yxz.y, 1.0f);  // Original X
    EXPECT_FLOAT_EQ(yxz.z, 3.0f);  // Z unchanged
}

TEST(VectorUtils, Vector3DSwizzleYZX_ReordersComponents)
{
    Vector3D xyz(1.0f, 2.0f, 3.0f);
    Vector3D yzx;

    Vector3DSwizzleYZX(yzx, xyz);

    EXPECT_FLOAT_EQ(yzx.x, 2.0f);  // Original Y
    EXPECT_FLOAT_EQ(yzx.y, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(yzx.z, 1.0f);  // Original X
}

TEST(VectorUtils, Vector3DSwizzleZXY_ReordersComponents)
{
    Vector3D xyz(1.0f, 2.0f, 3.0f);
    Vector3D zxy;

    Vector3DSwizzleZXY(zxy, xyz);

    EXPECT_FLOAT_EQ(zxy.x, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(zxy.y, 1.0f);  // Original X
    EXPECT_FLOAT_EQ(zxy.z, 2.0f);  // Original Y
}

TEST(VectorUtils, Vector3DSwizzleZYX_ReverseComponents)
{
    Vector3D xyz(1.0f, 2.0f, 3.0f);
    Vector3D zyx;

    Vector3DSwizzleZYX(zyx, xyz);

    EXPECT_FLOAT_EQ(zyx.x, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(zyx.y, 2.0f);  // Y unchanged
    EXPECT_FLOAT_EQ(zyx.z, 1.0f);  // Original X
}

// ==============================================================================
// Vector4D Swizzling Tests
// ==============================================================================

TEST(VectorUtils, Vector4DSwizzleXZY_ReordersXYZComponents)
{
    Vector4D xyzw(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D xzyw;

    Vector4DSwizzleXZY(xzyw, xyzw);

    EXPECT_FLOAT_EQ(xzyw.x, 1.0f);  // X unchanged
    EXPECT_FLOAT_EQ(xzyw.y, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(xzyw.z, 2.0f);  // Original Y
    EXPECT_FLOAT_EQ(xzyw.w, 4.0f);  // W unchanged
}

TEST(VectorUtils, Vector4DSwizzleYXZ_ReordersXYZComponents)
{
    Vector4D xyzw(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D yxzw;

    Vector4DSwizzleYXZ(yxzw, xyzw);

    EXPECT_FLOAT_EQ(yxzw.x, 2.0f);  // Original Y
    EXPECT_FLOAT_EQ(yxzw.y, 1.0f);  // Original X
    EXPECT_FLOAT_EQ(yxzw.z, 3.0f);  // Z unchanged
    EXPECT_FLOAT_EQ(yxzw.w, 4.0f);  // W unchanged
}

TEST(VectorUtils, Vector4DSwizzleYZX_ReordersXYZComponents)
{
    Vector4D xyzw(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D yzxw;

    Vector4DSwizzleYZX(yzxw, xyzw);

    EXPECT_FLOAT_EQ(yzxw.x, 2.0f);  // Original Y
    EXPECT_FLOAT_EQ(yzxw.y, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(yzxw.z, 1.0f);  // Original X
    EXPECT_FLOAT_EQ(yzxw.w, 4.0f);  // W unchanged
}

TEST(VectorUtils, Vector4DSwizzleZXY_ReordersXYZComponents)
{
    Vector4D xyzw(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D zxyw;

    Vector4DSwizzleZXY(zxyw, xyzw);

    EXPECT_FLOAT_EQ(zxyw.x, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(zxyw.y, 1.0f);  // Original X
    EXPECT_FLOAT_EQ(zxyw.z, 2.0f);  // Original Y
    EXPECT_FLOAT_EQ(zxyw.w, 4.0f);  // W unchanged
}

TEST(VectorUtils, Vector4DSwizzleZYX_ReverseXYZComponents)
{
    Vector4D xyzw(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4D zyxw;

    Vector4DSwizzleZYX(zyxw, xyzw);

    EXPECT_FLOAT_EQ(zyxw.x, 3.0f);  // Original Z
    EXPECT_FLOAT_EQ(zyxw.y, 2.0f);  // Y unchanged
    EXPECT_FLOAT_EQ(zyxw.z, 1.0f);  // Original X
    EXPECT_FLOAT_EQ(zyxw.w, 4.0f);  // W unchanged
}

// ==============================================================================
// Round-trip Conversion Tests
// ==============================================================================

TEST(VectorUtils, RoundTrip_Vector2DToVector3DAndBack)
{
    Vector2D original(5.0f, 10.0f);
    Vector3D v3;
    Vector2D result;

    ToVector3DFromVector2D(v3, original);
    ToVector2DFromVector3D(result, v3);

    EXPECT_FLOAT_EQ(result.x, original.x);
    EXPECT_FLOAT_EQ(result.y, original.y);
}

TEST(VectorUtils, RoundTrip_Vector3DToVector4DPointAndBack)
{
    Vector3D original(1.0f, 2.0f, 3.0f);
    Vector4D v4;
    Vector3D result;

    ToVector4DPointFromVector3D(v4, original);
    ToVector3DFromVector4D(result, v4);

    EXPECT_FLOAT_EQ(result.x, original.x);
    EXPECT_FLOAT_EQ(result.y, original.y);
    EXPECT_FLOAT_EQ(result.z, original.z);
}
