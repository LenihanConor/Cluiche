#ifndef DIA_GEOMETRY2D_TRIANGLE_H
#define DIA_GEOMETRY2D_TRIANGLE_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		class Circle;
		class Line;

		class Triangle
		{
		public:
			enum PtId
			{
				kPt0 = 0,
				kPt1,
				kPt2,

				kNumPts
			};

			enum EdgeId
			{
				kEdge0_Pt0ToPt1 = 0,
				kEdge1_Pt1ToPt2,
				kEdge2_Pt2ToPt0,

				kNumEdges
			};

			Triangle();
			Triangle(const Triangle& rhs);
			Triangle(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2, const Dia::Maths::Vector2D& pt3);

			Triangle& operator = (const Triangle& rhs);

			const Dia::Maths::Vector2D& GetPt(PtId index) const;
			const Dia::Maths::Vector2D& GetPt(int index) const;

			void CalculateEdge(EdgeId index, Line& result) const;
			void CalculateEdge(int index, Line& result) const;

			Dia::Maths::Vector2D CenterOfGravity() const;

			void CalculateInCircle(Circle& circle) const;
			void CalculateCircumCircle(Circle& circle) const;

			void ClosestPointOnOORectTo(const Circle& circle, Dia::Maths::Vector2D& result) const;

			IntersectionClassify IsIntersecting(const Circle& circle) const;

		private:
			Dia::Maths::Vector2D mPts[kNumPts];
		};
	}
}

#include "DiaGeometry2D/Shapes/Triangle.inl"

#endif // DIA_GEOMETRY2D_TRIANGLE_H
