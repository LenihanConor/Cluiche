#include "DiaGeometry2D/Shapes/OORect.h"

#include "DiaGeometry2D/Intersection/IntersectionTests.h"
#include "DiaGeometry2D/Shapes/Line.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		void OORect::CalculateEdge(EdgeId index, Line& result)const
		{
			DIA_ASSERT(index >= 0 && index < kNumEdges, "Invalid index");

			switch(index)
			{
				case kEdge0_Pt0ToPt1: result.Create(GetPt(kPt0), GetPt(kPt1)); break;
				case kEdge1_Pt1ToPt2: result.Create(GetPt(kPt1), GetPt(kPt2)); break;
				case kEdge2_Pt2ToPt3: result.Create(GetPt(kPt2), GetPt(kPt3)); break;
				case kEdge3_Pt3ToPt0: result.Create(GetPt(kPt3), GetPt(kPt0)); break;
				default: DIA_ASSERT(0, "Invalid Entry in switch"); break;
			}
		}

		//-----------------------------------------------------------------------------
		void OORect::CalculateEdge(int index, Line& result)const
		{
			DIA_ASSERT(index >= 0 && index < kNumEdges, "Invalid index");

			switch(index)
			{
				case kEdge0_Pt0ToPt1: result.Create(GetPt(kPt0), GetPt(kPt1)); break;
				case kEdge1_Pt1ToPt2: result.Create(GetPt(kPt1), GetPt(kPt2)); break;
				case kEdge2_Pt2ToPt3: result.Create(GetPt(kPt2), GetPt(kPt3)); break;
				case kEdge3_Pt3ToPt0: result.Create(GetPt(kPt3), GetPt(kPt0)); break;
				default: DIA_ASSERT(0, "Invalid Entry in switch"); break;
			}
		}

		//-----------------------------------------------------------------------------
		void OORect::ClosestPointOnOORectTo(const Circle& circle, Dia::Maths::Vector2D& result)const
		{
			DIA_ASSERT(IsIntersecting(circle).IsNotIntersecting(), "Circle intersection rect");

			float closestManhattenDist = Dia::Maths::Float::Max();

			for (int i = 0; i < OORect::kNumEdges; i++)
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

		//-----------------------------------------------------------------------------
		IntersectionClassify OORect::IsIntersecting(const Circle& circle)const
		{
			return IntersectionTests::IsIntersecting(*this, circle).ReInterpretAandBObject();
		}
	}
}
