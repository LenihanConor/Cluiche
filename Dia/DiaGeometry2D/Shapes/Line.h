#ifndef DIA_GEOMETRY2D_LINE_H
#define DIA_GEOMETRY2D_LINE_H

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		class Circle;

		class Line
		{
		public:
			Line();
			Line(const Line& rhs);
			Line(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2);

			Line&						operator =		(const Line& rhs);
			Dia::Maths::Vector2D&		operator[]		(int index);
			const Dia::Maths::Vector2D&	operator[]		(int index) const;
			Dia::Maths::Vector2D&		operator[]		(unsigned int index);
			const Dia::Maths::Vector2D&	operator[]		(unsigned int index) const;

			void Create(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2);

			const Dia::Maths::Vector2D&		GetPt1() const;
			const Dia::Maths::Vector2D&		GetPt2() const;

			Dia::Maths::Vector2D	Pt1ToPt2Vector() const;
			Dia::Maths::Vector2D	Pt2ToPt1Vector() const;

			float					Length()		const;
			float					SquareLength()	const;
			Dia::Maths::Vector2D	CalculateCenter() const;

			void ClosestPointOnLineTo(const Dia::Maths::Vector2D& point, Dia::Maths::Vector2D& result) const;
			void ClosestPointOnLineTo(const Circle& circle, Dia::Maths::Vector2D& result) const;

			IntersectionClassify IsIntersecting(const Dia::Maths::Vector2D& point) const;
			IntersectionClassify IsIntersecting(const Circle& rhs) const;

		private:
			static const int kNumPts = 2;
			Dia::Maths::Vector2D mPt[kNumPts];
		};
	}
}

#include "DiaGeometry2D/Shapes/Line.inl"

#endif // DIA_GEOMETRY2D_LINE_H
