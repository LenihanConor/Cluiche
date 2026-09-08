#include "DiaGeometry2D/Shapes/SplineFactory.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Geometry2D
	{
		Spline SplineFactory::MakeBSpline(const Dia::Maths::Vector2D* points, int count)
		{
			DIA_ASSERT(count >= 4, "BSpline requires at least 4 control points");
			DIA_ASSERT(count <= Spline::kMaxControlPoints, "BSpline exceeds kMaxControlPoints");

			Spline s;
			s.mCurveType = Spline::CurveType::BSpline;
			s.mControlPointCount = count;
			for (int i = 0; i < count; ++i)
				s.mControlPoints[i] = points[i];
			return s;
		}

		Spline SplineFactory::MakeCatmullRom(const Dia::Maths::Vector2D* points, int count)
		{
			DIA_ASSERT(count >= 4, "CatmullRom requires at least 4 control points");
			DIA_ASSERT(count <= Spline::kMaxControlPoints, "CatmullRom exceeds kMaxControlPoints");

			Spline s;
			s.mCurveType = Spline::CurveType::CatmullRom;
			s.mControlPointCount = count;
			for (int i = 0; i < count; ++i)
				s.mControlPoints[i] = points[i];
			return s;
		}
	}
}
