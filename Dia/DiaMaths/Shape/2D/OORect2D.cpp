#include "DiaMaths/Shape/2D/OORect2D.h"

#include "DiaMaths/Shape/Common/IntersectionTests.h"
#include "DiaMaths/Shape/2D/Line2D.h"
#include "DiaMaths/Shape/2D/Circle2D.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		void OORect2D::CalculateEdge(EdgeId index, Line2D& result)const
		{
			DIA_ASSERT(index >= 0 && index < kNumEdges, "Invalid index");

			switch(index)
			{
				case kEdge0_Pt0ToPt1: result.Create(GetPt(kPt0), GetPt(kPt1)); break;
				case kEdge1_Pt1ToPt2: result.Create(GetPt(kPt1), GetPt(kPt2)); break;
				case kEdge2_Pt2ToPt3: result.Create(GetPt(kPt2), GetPt( kPt3)); break;
				case kEdge3_Pt3ToPt0: result.Create(GetPt(kPt3), GetPt(kPt0)); break;
				default: DIA_ASSERT(0, "Invalid Entry in switch"); break;
			}
		}

		//-----------------------------------------------------------------------------
		void OORect2D::CalculateEdge(int index, Line2D& result)const
		{
			DIA_ASSERT(index >= 0 && index < kNumEdges, "Invalid index");

			switch(index)
			{
			case kEdge0_Pt0ToPt1: result.Create(GetPt(kPt0), GetPt(kPt1)); break;
			case kEdge1_Pt1ToPt2: result.Create(GetPt(kPt1), GetPt(kPt2)); break;
			case kEdge2_Pt2ToPt3: result.Create(GetPt(kPt2), GetPt( kPt3)); break;
			case kEdge3_Pt3ToPt0: result.Create(GetPt(kPt3), GetPt(kPt0)); break;
			default: DIA_ASSERT(0, "Invalid Entry in switch"); break;
			}
		}

		//-----------------------------------------------------------------------------
		void OORect2D::ClosestPointOnOORectTo(const Circle2D& circle, Vector2D& result)const
		{
			DIA_ASSERT(IsIntersecting(circle).IsNotIntersecting(), "Circle intersection rect");

			float closestManhattenDist = Float::Max();

			for (int i = 0; i < OORect2D::kNumEdges; i++)
			{
				Vector2D closestPt;
				Line2D edge;

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

		//-----------------------------------------------------------------------------
		IntersectionClassify OORect2D::IsIntersecting(const Circle2D& circle)const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}