#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Shape/Common/IntersectionClassify.h"
namespace Dia
{
	namespace Maths
	{
		class Circle2D;
		class Line2D;

		class OORect2D
		{
		public:
			enum PtId
			{
				kPt0 = 0,
				kPt1,
				kPt2,
				kPt3,

				kNumPts
			};

			enum EdgeId
			{
				kEdge0_Pt0ToPt1 = 0,
				kEdge1_Pt1ToPt2,
				kEdge2_Pt2ToPt3,
				kEdge3_Pt3ToPt0,

				kNumEdges
			};

			OORect2D();
			OORect2D(const Vector2D& pt1, const Vector2D& pt2, const Vector2D& pt3, const Vector2D& pt4);
			OORect2D(const OORect2D& rhs);

			OORect2D&		operator =			(const OORect2D& rhs);
			
			const Vector2D&	GetPt(PtId index)const;
			const Vector2D&	GetPt(int index)const;

			void		CalculateEdge(EdgeId index, Line2D& result)const;
			void		CalculateEdge(int index, Line2D& result)const;

			float		CalculateRadius()const;
			float		CalculateSquareRadius()const;
			Vector2D	CalculateCenter()const;

			void ClosestPointOnOORectTo(const Circle2D& rect, Vector2D& result)const;
			
			//IntersectionClassify IsIntersecting(const Vector2D& point)const;
			//IntersectionClassify IsIntersecting(const AARect2D& rect)const;
			//IntersectionClassify IsIntersecting(const Arc2D& arc)const;
			//IntersectionClassify IsIntersecting(const Capsule2D& capsule)const;
			IntersectionClassify IsIntersecting(const Circle2D& circle)const;
			//IntersectionClassify IsIntersecting(const Line2D& line)const;
			//IntersectionClassify IsIntersecting(const OORect2D& rhs)const;
			//IntersectionClassify IsIntersecting(const Triangle2D& rhs)const;
		private:
			Vector2D mPts[kNumPts];
		};
	}
}

#include "DiaMaths/Shape/2D/OORect2D.inl"