#include "DiaGeometry2D/Shapes/Triangle.h"

#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Line.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//----------------------------------------------------------------------------------------------------
		Dia::Maths::Vector2D Triangle::CenterOfGravity()const
		{
			return Dia::Maths::Vector2D((mPts[0] + mPts[1] + mPts[2]) / 3.0f);
		}

		//----------------------------------------------------------------------------------------------------
		void Triangle::CalculateInCircle(Circle& circle)const
		{
			float l0 = (mPts[1] - mPts[2]).Magnitude();
			float l1 = (mPts[0] - mPts[2]).Magnitude();
			float l2 = (mPts[0] - mPts[1]).Magnitude();

			float perimeter = l0 + l1 + l2;
			float s = perimeter / 2.0f;
			float area = Dia::Maths::SquareRoot(s * (s - l0) * (s - l1) * (s - l2));

			circle.SetCenter(((mPts[0] * l0) + (mPts[1] * l1) + (mPts[2] * l2)) / perimeter);
			circle.SetRadius(area / perimeter);
		}

		//----------------------------------------------------------------------------------------------------
		void Triangle::CalculateCircumCircle(Circle& circle)const
		{
			Dia::Maths::Vector2D translatePt1(-mPts[0]);

			Dia::Maths::Vector2D pt1(0.0f, 0.0f);
			Dia::Maths::Vector2D pt2(mPts[1] + translatePt1);
			Dia::Maths::Vector2D pt3(mPts[2] + translatePt1);

			float pt2Squared = Dia::Maths::Square(pt2.x) + Dia::Maths::Square(pt2.y);
			float pt3Squared = Dia::Maths::Square(pt3.x) + Dia::Maths::Square(pt3.y);

			float D = 2.0f * ((pt2.x * pt3.y) - (pt2.y * pt3.x));

			Dia::Maths::Vector2D center;
			center.x = ((pt3.y * pt2Squared) - (pt2.y * pt3Squared)) / D;
			center.y = ((pt2.x * pt3Squared) - (pt3.x * pt2Squared)) / D;

			center -= translatePt1;

			circle.SetRadius((center - mPts[0]).Magnitude());
			circle.SetCenter(center);
		}

		//-----------------------------------------------------------------------------
		void Triangle::CalculateEdge(EdgeId index, Line& result)const
		{
			DIA_ASSERT(index >= 0 && index < kNumEdges, "Invalid index");

			switch(index)
			{
				case kEdge0_Pt0ToPt1: result.Create(GetPt(kPt0), GetPt(kPt1)); break;
				case kEdge1_Pt1ToPt2: result.Create(GetPt(kPt1), GetPt(kPt2)); break;
				case kEdge2_Pt2ToPt0: result.Create(GetPt(kPt2), GetPt(kPt0)); break;
				default: DIA_ASSERT(0, "Invalid Entry in switch"); break;
			}
		}

		//-----------------------------------------------------------------------------
		void Triangle::CalculateEdge(int index, Line& result)const
		{
			DIA_ASSERT(index >= 0 && index < kNumEdges, "Invalid index");

			switch(index)
			{
				case kEdge0_Pt0ToPt1: result.Create(GetPt(kPt0), GetPt(kPt1)); break;
				case kEdge1_Pt1ToPt2: result.Create(GetPt(kPt1), GetPt(kPt2)); break;
				case kEdge2_Pt2ToPt0: result.Create(GetPt(kPt2), GetPt(kPt0)); break;
				default: DIA_ASSERT(0, "Invalid Entry in switch"); break;
			}
		}

		//-----------------------------------------------------------------------------
		void Triangle::ClosestPointOnOORectTo(const Circle& circle, Dia::Maths::Vector2D& result)const
		{
			DIA_ASSERT(IsIntersecting(circle).IsNotIntersecting(), "Circle intersection triangle");

			float closestManhattenDist = Dia::Maths::Float::Max();

			for (int i = 0; i < Triangle::kNumEdges; i++)
			{
				Dia::Maths::Vector2D closestPt;
				Line edge;

				CalculateEdge(static_cast<EdgeId>(i), edge);

				edge.ClosestPointOnLineTo(circle.GetCenter(), closestPt);

				float currentManhattenDist = closestPt.ManhattanDistanceTo(circle.GetCenter());

				if (currentManhattenDist < closestManhattenDist)
				{
					closestManhattenDist = currentManhattenDist;
					result = closestPt;
				}
			}
		}

		//----------------------------------------------------------------------------------------------------
		IntersectionClassify Triangle::IsIntersecting(const Circle& circle)const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}
