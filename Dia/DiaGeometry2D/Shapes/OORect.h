#ifndef DIA_GEOMETRY2D_OORECT_H
#define DIA_GEOMETRY2D_OORECT_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		class Circle;
		class Line;

		class OORect
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

			OORect();
			OORect(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2, const Dia::Maths::Vector2D& pt3, const Dia::Maths::Vector2D& pt4);
			OORect(const OORect& rhs);

			OORect& operator = (const OORect& rhs);

			const Dia::Maths::Vector2D&	GetPt(PtId index) const;
			const Dia::Maths::Vector2D&	GetPt(int index) const;

			void CalculateEdge(EdgeId index, Line& result) const;
			void CalculateEdge(int index, Line& result) const;

			float				CalculateRadius()		const;
			float				CalculateSquareRadius()	const;
			Dia::Maths::Vector2D CalculateCenter()		const;

			void ClosestPointOnOORectTo(const Circle& circle, Dia::Maths::Vector2D& result) const;

			IntersectionClassify IsIntersecting(const Circle& circle) const;

		private:
			Dia::Maths::Vector2D mPts[kNumPts];
		};
	}
}

#include "DiaGeometry2D/Shapes/OORect.inl"

#endif // DIA_GEOMETRY2D_OORECT_H
