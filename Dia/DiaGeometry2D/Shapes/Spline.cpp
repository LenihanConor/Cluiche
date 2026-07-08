#include "DiaGeometry2D/Shapes/Spline.h"

#include <DiaMaths/Core/SplineBasis.h>
#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia
{
	namespace Geometry2D
	{
		// ---------------------------------------------------------------------------
		// Segment helpers — static, file-local
		// ---------------------------------------------------------------------------

		static Dia::Maths::Vector2D EvalBSplineSegment(
			const Dia::Maths::Vector2D& p0,
			const Dia::Maths::Vector2D& p1,
			const Dia::Maths::Vector2D& p2,
			const Dia::Maths::Vector2D& p3,
			float u)
		{
			float b0, b1, b2, b3;
			Dia::Maths::CubicBSplineBasis(u, b0, b1, b2, b3);
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
			float b0, b1, b2, b3;
			Dia::Maths::CubicBSplineBasisTangent(u, b0, b1, b2, b3);
			return Dia::Maths::Vector2D(
				b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x,
				b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y);
		}

		static Dia::Maths::Vector2D EvalCatmullRomSegment(
			const Dia::Maths::Vector2D& p0,
			const Dia::Maths::Vector2D& p1,
			const Dia::Maths::Vector2D& p2,
			const Dia::Maths::Vector2D& p3,
			float u)
		{
			float b0, b1, b2, b3;
			Dia::Maths::CatmullRomBasis(u, b0, b1, b2, b3);
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
			float b0, b1, b2, b3;
			Dia::Maths::CatmullRomBasisTangent(u, b0, b1, b2, b3);
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
