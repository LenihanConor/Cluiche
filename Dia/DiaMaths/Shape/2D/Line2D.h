#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Shape/Common/IntersectionClassify.h"
namespace Dia
{
	namespace Maths
	{
		class AARect2D;
		class Arc2D;
		class Capsule2D;
		class Circle2D;
		class Line2D;
		class OORect2D;
		class Triangle2D;

		class Line2D
		{
		public:
			Line2D();

			Line2D(const Line2D& rhs);
			Line2D(const Vector2D& pt1, const Vector2D& pt2);

			Line2D&				operator =		(const Line2D& rhs);
			Vector2D&			operator[]		(int index);										
			const Vector2D&		operator[]		(int index) const;	
			Vector2D&			operator[]		(unsigned int index);										
			const Vector2D&		operator[]		(unsigned int index) const;	

			void Create(const Vector2D& pt1, const Vector2D& pt2);

			const Vector2D&		GetPt1()const;
			const Vector2D&		GetPt2()const;
			
			Vector2D			Pt1ToPt2Vector()const;
			Vector2D			Pt2ToPt1Vector()const;
			
			float				Length() const;
			float				SquareLength() const;
			Vector2D			CalculateCenter()const;

			void ClosestPointOnLineTo(const Vector2D& point, Vector2D& result)const;
			//bool ClosestPointTo(const AARect2D& point, Vector2D& result)const;
			//bool ClosestPointTo(const Arc2D& rhsv)const;
			//bool ClosestPointTo(const Capsule2D& rhs, Vector2D& result)const;
			//bool ClosestPointTo(const Circle2D& point, Vector2D& result)const;
			void ClosestPointOnLineTo(const Circle2D& line, Vector2D& result)const;
			//bool ClosestPointTo(const OORect2D& rhs, Vector2D& result)const;
			//bool ClosestPointTo(const Triangle2D& rhs, Vector2D& result)const;

			IntersectionClassify IsIntersecting(const Vector2D& point)const;
			//IntersectionResult2D IsIntersecting(const AARect2D& rhs)const;
			//IntersectionResult2D IsIntersecting(const Arc2D& rhs)const;
			//IntersectionResult2D IsIntersecting(const Capsule2D& rhs)const;
			IntersectionClassify IsIntersecting(const Circle2D& rhs)const;
			//IntersectionClassify IsIntersecting(const Line2D& line)const;
			//IntersectionResult2D IsIntersecting(const OORect2D& rhs)const;
			//IntersectionResult2D IsIntersecting(const Triangle2D& rhs)const;
		private:
			static const int kNumPts = 2;
			Vector2D mPt[kNumPts];
		};
	}
}

#include "DiaMaths/Shape/2D/Line2D.inl"