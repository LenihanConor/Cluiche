#ifndef DIA_GEOMETRY2D_SPLINEFACTORY_H
#define DIA_GEOMETRY2D_SPLINEFACTORY_H

#include "DiaGeometry2D/Shapes/Spline.h"

namespace Dia
{
	namespace Geometry2D
	{
		class SplineFactory
		{
		public:
			// Uniform cubic B-Spline. Smooth approximation; does not pass through points.
			// Requires count >= 4 and count <= Spline::kMaxControlPoints.
			static Spline MakeBSpline(const Dia::Maths::Vector2D* points, int count);

			// Catmull-Rom spline. Interpolating; passes through every control point.
			// Requires count >= 4 and count <= Spline::kMaxControlPoints.
			static Spline MakeCatmullRom(const Dia::Maths::Vector2D* points, int count);

		private:
			SplineFactory() = delete;
		};
	}
}

#endif // DIA_GEOMETRY2D_SPLINEFACTORY_H
