#include "DiaGeometry2D/Shapes/Spline.h"

#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia
{
	namespace Geometry2D
	{
		// ---------------------------------------------------------------------------
		// Basis helpers — static, file-local
		// ---------------------------------------------------------------------------

		// Uniform cubic B-Spline basis matrix (row-vector form: [t^3 t^2 t 1] * M * P)
		// M_bs = (1/6) * [[-1  3 -3  1]
		//                 [ 3 -6  3  0]
		//                 [-3  0  3  0]
		//                 [ 1  4  1  0]]
		static Dia::Maths::Vector2D EvalBSplineSegment(
			const Dia::Maths::Vector2D& p0,
			const Dia::Maths::Vector2D& p1,
			const Dia::Maths::Vector2D& p2,
			const Dia::Maths::Vector2D& p3,
			float u)
		{
			const float u2 = u * u;
			const float u3 = u2 * u;

			const float b0 = (1.0f / 6.0f) * (-u3 + 3.0f*u2 - 3.0f*u + 1.0f);
			const float b1 = (1.0f / 6.0f) * (3.0f*u3 - 6.0f*u2 + 4.0f);
			const float b2 = (1.0f / 6.0f) * (-3.0f*u3 + 3.0f*u2 + 3.0f*u + 1.0f);
			const float b3 = (1.0f / 6.0f) * u3;

			return Dia::Maths::Vector2D(
				b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x,
				b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y);
		}

		static Dia::Maths::Vector2D EvalBSplineTangentSegment(
			const Dia::Maths::Vector2D& p0,
			const Dia::Maths::Vector2D& p1,
			const Dia::Maths::Vector2D& p2,
			const Dia::Maths::Vector2D& p3,
			float u)
		{
			const float u2 = u * u;

			const float b0 = (1.0f / 6.0f) * (-3.0f*u2 + 6.0f*u - 3.0f);
			const float b1 = (1.0f / 6.0f) * (9.0f*u2 - 12.0f*u);
			const float b2 = (1.0f / 6.0f) * (-9.0f*u2 + 6.0f*u + 3.0f);
			const float b3 = (1.0f / 6.0f) * (3.0f*u2);

			return Dia::Maths::Vector2D(
				b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x,
				b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y);
		}

		// Catmull-Rom basis (tension = 0.5)
		// M_cr = 0.5 * [[-1  3 -3  1]
		//               [ 2 -5  4 -1]
		//               [-1  0  1  0]
		//               [ 0  2  0  0]]
		static Dia::Maths::Vector2D EvalCatmullRomSegment(
			const Dia::Maths::Vector2D& p0,
			const Dia::Maths::Vector2D& p1,
			const Dia::Maths::Vector2D& p2,
			const Dia::Maths::Vector2D& p3,
			float u)
		{
			const float u2 = u * u;
			const float u3 = u2 * u;

			const float b0 = 0.5f * (-u3 + 2.0f*u2 - u);
			const float b1 = 0.5f * (3.0f*u3 - 5.0f*u2 + 2.0f);
			const float b2 = 0.5f * (-3.0f*u3 + 4.0f*u2 + u);
			const float b3 = 0.5f * (u3 - u2);

			return Dia::Maths::Vector2D(
				b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x,
				b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y);
		}

		static Dia::Maths::Vector2D EvalCatmullRomTangentSegment(
			const Dia::Maths::Vector2D& p0,
			const Dia::Maths::Vector2D& p1,
			const Dia::Maths::Vector2D& p2,
			const Dia::Maths::Vector2D& p3,
			float u)
		{
			const float u2 = u * u;

			const float b0 = 0.5f * (-3.0f*u2 + 4.0f*u - 1.0f);
			const float b1 = 0.5f * (9.0f*u2 - 10.0f*u);
			const float b2 = 0.5f * (-9.0f*u2 + 8.0f*u + 1.0f);
			const float b3 = 0.5f * (3.0f*u2 - 2.0f*u);

			return Dia::Maths::Vector2D(
				b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x,
				b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y);
		}

		// ---------------------------------------------------------------------------
		// Spline
		// ---------------------------------------------------------------------------

		Spline::Spline()
			: mControlPointCount(0)
			, mCurveType(CurveType::BSpline)
		{}

		int Spline::GetControlPointCount() const
		{
			return mControlPointCount;
		}

		const Dia::Maths::Vector2D& Spline::GetControlPoint(int index) const
		{
			DIA_ASSERT(index >= 0 && index < mControlPointCount, "Spline control point index out of range");
			return mControlPoints[index];
		}

		Spline::CurveType Spline::GetCurveType() const
		{
			return mCurveType;
		}

		Dia::Maths::Vector2D Spline::Evaluate(float t) const
		{
			DIA_ASSERT(mControlPointCount >= 4, "Spline requires at least 4 control points");

			// Clamp t to [0,1]
			if (t <= 0.0f) t = 0.0f;
			if (t >= 1.0f) t = 1.0f;

			if (mCurveType == CurveType::BSpline)
			{
				// n-3 segments for n control points
				const int segCount = mControlPointCount - 3;
				const float scaled = t * static_cast<float>(segCount);
				int seg = static_cast<int>(scaled);
				if (seg >= segCount) seg = segCount - 1;
				const float u = scaled - static_cast<float>(seg);

				return EvalBSplineSegment(
					mControlPoints[seg],
					mControlPoints[seg + 1],
					mControlPoints[seg + 2],
					mControlPoints[seg + 3],
					u);
			}
			else // CatmullRom
			{
				// n-1 segments; each segment uses a 4-point stencil clamped at ends
				const int segCount = mControlPointCount - 1;
				const float scaled = t * static_cast<float>(segCount);
				int seg = static_cast<int>(scaled);
				if (seg >= segCount) seg = segCount - 1;
				const float u = scaled - static_cast<float>(seg);

				const int i0 = (seg > 0) ? seg - 1 : 0;
				const int i1 = seg;
				const int i2 = seg + 1;
				const int i3 = (seg + 2 < mControlPointCount) ? seg + 2 : mControlPointCount - 1;

				return EvalCatmullRomSegment(
					mControlPoints[i0],
					mControlPoints[i1],
					mControlPoints[i2],
					mControlPoints[i3],
					u);
			}
		}

		Dia::Maths::Vector2D Spline::EvaluateTangent(float t) const
		{
			DIA_ASSERT(mControlPointCount >= 4, "Spline requires at least 4 control points");

			if (t <= 0.0f) t = 0.0f;
			if (t >= 1.0f) t = 1.0f;

			Dia::Maths::Vector2D tangent;

			if (mCurveType == CurveType::BSpline)
			{
				const int segCount = mControlPointCount - 3;
				const float scaled = t * static_cast<float>(segCount);
				int seg = static_cast<int>(scaled);
				if (seg >= segCount) seg = segCount - 1;
				const float u = scaled - static_cast<float>(seg);

				tangent = EvalBSplineTangentSegment(
					mControlPoints[seg],
					mControlPoints[seg + 1],
					mControlPoints[seg + 2],
					mControlPoints[seg + 3],
					u);
			}
			else // CatmullRom
			{
				const int segCount = mControlPointCount - 1;
				const float scaled = t * static_cast<float>(segCount);
				int seg = static_cast<int>(scaled);
				if (seg >= segCount) seg = segCount - 1;
				const float u = scaled - static_cast<float>(seg);

				const int i0 = (seg > 0) ? seg - 1 : 0;
				const int i1 = seg;
				const int i2 = seg + 1;
				const int i3 = (seg + 2 < mControlPointCount) ? seg + 2 : mControlPointCount - 1;

				tangent = EvalCatmullRomTangentSegment(
					mControlPoints[i0],
					mControlPoints[i1],
					mControlPoints[i2],
					mControlPoints[i3],
					u);
			}

			const float len = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
			if (len > 1e-6f)
			{
				tangent.x /= len;
				tangent.y /= len;
			}
			return tangent;
		}
	}
}
