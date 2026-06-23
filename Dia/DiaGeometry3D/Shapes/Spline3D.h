#ifndef DIA_GEOMETRY3D_SPLINE3D_H
#define DIA_GEOMETRY3D_SPLINE3D_H

#include "DiaMaths/Vector/Vector3D.h"

namespace Dia
{
	namespace Geometry3D
	{
		class SplineFactory3D;

		class Spline3D
		{
		public:
			static constexpr int kMaxControlPoints = 16;

			enum class CurveType { BSpline, CatmullRom };

			Spline3D(); // invalid until constructed by SplineFactory3D

			int                          GetControlPointCount() const;
			const Dia::Maths::Vector3D&  GetControlPoint(int index) const;
			CurveType                    GetCurveType() const;

			// t in [0,1] — clamped at endpoints
			Dia::Maths::Vector3D Evaluate(float t) const;
			// Normalised tangent direction at t
			Dia::Maths::Vector3D EvaluateTangent(float t) const;

		private:
			friend class SplineFactory3D;

			Dia::Maths::Vector3D mControlPoints[kMaxControlPoints];
			int                  mControlPointCount;
			CurveType            mCurveType;
		};

		class SplineFactory3D
		{
		public:
			// Uniform cubic B-spline: smooth approximation, does NOT pass through control points.
			// Precondition: count >= 4 && count <= Spline3D::kMaxControlPoints
			static Spline3D MakeBSpline(const Dia::Maths::Vector3D* points, int count);
			// Catmull-Rom: interpolating — passes through every control point.
			// Precondition: count >= 4 && count <= Spline3D::kMaxControlPoints
			static Spline3D MakeCatmullRom(const Dia::Maths::Vector3D* points, int count);

		private:
			SplineFactory3D() = delete;
		};
	}
}

#endif // DIA_GEOMETRY3D_SPLINE3D_H
