#pragma once
// =============================================================================
// DiaGeometry2DSerializers.h
// serialize() free functions for DiaGeometry2D shape types.
//
// USAGE:
//   #include "DiaGeometry2D/DiaGeometry2DSerializers.h"
//
// NOTE: Arc and Sector require Angle serialization. Include
//   DiaMaths/DiaMathsSerializers.h before this header (or include it
//   alongside) if Angle round-trips are needed in the same TU.
//
// ADL NOTE:
//   All serialize() functions are in namespace Dia::Geometry2D, matching
//   the types so ADL finds them automatically.
// =============================================================================

#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaMaths/DiaMathsSerializers.h"
#include "DiaGeometry2D/Shapes/Point.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/OORect.h"
#include "DiaGeometry2D/Shapes/Line.h"
#include "DiaGeometry2D/Shapes/Ray.h"
#include "DiaGeometry2D/Shapes/Triangle.h"
#include "DiaGeometry2D/Shapes/Capsule.h"
#include "DiaGeometry2D/Shapes/Arc.h"
#include "DiaGeometry2D/Shapes/Sector.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"

namespace Dia::Geometry2D {

// -----------------------------------------------------------------------------
// Point  — private Vector2D mPosition
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Point, 1)
    DIA_FIELD(mPosition)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Circle  — private Vector2D mCenter, float mRadius
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Circle, 1)
    DIA_FIELD(mCenter)
    DIA_FIELD(mRadius)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// AARect  — private Vector2D mBottomLeft, mTopRight
// -----------------------------------------------------------------------------
DIA_SERIALIZE(AARect, 1)
    DIA_FIELD(mBottomLeft)
    DIA_FIELD(mTopRight)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// OORect  — private Vector2D mPts[4]
// -----------------------------------------------------------------------------
DIA_SERIALIZE(OORect, 1)
    DIA_FIELD_NAMED("pt0", mPts[0])
    DIA_FIELD_NAMED("pt1", mPts[1])
    DIA_FIELD_NAMED("pt2", mPts[2])
    DIA_FIELD_NAMED("pt3", mPts[3])
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Line  — private Vector2D mPt[2]
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Line, 1)
    DIA_FIELD_NAMED("pt1", mPt[0])
    DIA_FIELD_NAMED("pt2", mPt[1])
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Ray  — private Vector2D mOrigin, mDirection (direction always normalized)
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Ray, 1)
    DIA_FIELD(mOrigin)
    DIA_FIELD(mDirection)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Triangle  — private Vector2D mPts[3]
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Triangle, 1)
    DIA_FIELD_NAMED("pt0", mPts[0])
    DIA_FIELD_NAMED("pt1", mPts[1])
    DIA_FIELD_NAMED("pt2", mPts[2])
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Capsule  — private Vector2D mPt1, mPt2; float mRadius
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Capsule, 1)
    DIA_FIELD(mPt1)
    DIA_FIELD(mPt2)
    DIA_FIELD(mRadius)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Arc  — private float mRadius; Angle mAngle; Vector2D mFocal, mAxis
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Arc, 1)
    DIA_FIELD(mRadius)
    DIA_FIELD(mAngle)
    DIA_FIELD(mFocal)
    DIA_FIELD(mAxis)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// Sector  — private Vector2D mCenter; float mRadius; Vector2D mAxis; Angle mHalfAngle
// -----------------------------------------------------------------------------
DIA_SERIALIZE(Sector, 1)
    DIA_FIELD(mCenter)
    DIA_FIELD(mRadius)
    DIA_FIELD(mAxis)
    DIA_FIELD(mHalfAngle)
DIA_SERIALIZE_END

// -----------------------------------------------------------------------------
// ConvexPolygon  — private Vector2D mVertices[16]; int mVertexCount
//   Only the live [0..mVertexCount-1] slice is serialized; the remainder of
//   the fixed array is not written and stays zero-initialised on read.
// -----------------------------------------------------------------------------
DIA_SERIALIZE(ConvexPolygon, 1)
    DIA_FIELD(mVertexCount)
    for (int _i = 0; _i < obj.mVertexCount; ++_i) {
        // Use a per-index name so each vertex maps to a stable JSON key
        char _name[8];
        _name[0] = 'v'; _name[1] = '0' + (char)(_i / 10); _name[2] = '0' + (char)(_i % 10); _name[3] = '\0';
        _ar_ & Dia::Reflect::named(_name, obj.mVertices[_i]);
    }
DIA_SERIALIZE_END

}  // namespace Dia::Geometry2D
