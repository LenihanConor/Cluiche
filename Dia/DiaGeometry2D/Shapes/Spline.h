#ifndef DIA_GEOMETRY2D_SPLINE_H
#define DIA_GEOMETRY2D_SPLINE_H

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Geometry2D
	{
		class SplineFactory;

		class Spline
		{
		public:
			static constexpr int kMaxControlPoints = 16;

			enum class CurveType { BSpline, CatmullRom };

			Spline(); // empty/invalid state; use SplineFactory for valid splines

			int                          GetControlPointCount() const;
			const Dia::Maths::Vector2D&  GetControlPoint(int index) const;
			CurveType                    GetCurveType() const;

			// t in [0,1]. DIA_ASSERTs if control point count < 4.
			Dia::Maths::Vector2D Evaluate(float t) const;
			// Returns normalised tangent. DIA_ASSERTs if control point count < 4.
			Dia::Maths::Vector2D EvaluateTangent(float t) const;

		private:
			friend class SplineFactory;

			Dia::Maths::Vector2D mControlPoints[kMaxControlPoints];
			int                  mControlPointCount;
			CurveType            mCurveType;
		};
	}
}

#endif // DIA_GEOMETRY2D_SPLINE_H
