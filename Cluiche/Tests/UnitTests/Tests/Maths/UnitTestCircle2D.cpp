
#include "UnitTests/Tests/Maths/UnitTestCircle2D.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Shape/2D/Circle2D.h>
#include <DiaMaths/Shape/2D/Line2D.h>
#include <DiaMaths/Shape/2D/Capsule2D.h>
#include <DiaMaths/Shape/2D/AARect2D.h>
#include <DiaMaths/Shape/2D/OORect2D.h>
#include <DiaMaths/Shape/2D/Triangle2D.h>
#include <DiaMaths/Shape/2D/Arc2D.h>

using Dia::Maths::Angle;
using Dia::Maths::Circle2D;
using Dia::Maths::Vector2D;
using Dia::Maths::AARect2D;
using Dia::Maths::Line2D;
using Dia::Maths::Arc2D;
using Dia::Maths::Capsule2D;
using Dia::Maths::OORect2D;
using Dia::Maths::Triangle2D;

namespace UnitTests
{	
	UnitTestCircle2D::UnitTestCircle2D(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestCircle2D::UnitTestCircle2D(void)
		: UnitTestMaths()
	{}

	void UnitTestCircle2D::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Circle2D circle;

			UNIT_TEST_POSITIVE(circle.GetRadius() == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Circle2D circle(1.0f, Vector2D(2.0f, 3.0f));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 3.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle1(1.0f, Vector2D(2.0f, 3.0f));
			Circle2D circle2(circle1);

			UNIT_TEST_POSITIVE(circle2.GetRadius() == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle2.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle2.GetCenter().y == 3.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle1(1.0f, Vector2D(2.0f, 3.0f));
			Circle2D circle2 = circle1;

			UNIT_TEST_POSITIVE(circle2.GetRadius() == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle2.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle2.GetCenter().y == 3.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle1(1.0f, Vector2D(2.0f, 3.0f));
			
			Circle2D circle2 = circle1;
			
			Circle2D circle3(2.0f, Vector2D(2.0f, 3.0f));
			Circle2D circle4(1.0f, Vector2D(1.0f, 3.0f));
			Circle2D circle5(1.0f, Vector2D(2.0f, 1.0f));
			
			UNIT_TEST_POSITIVE(circle1 == circle2, "Circle2D");
			UNIT_TEST_POSITIVE(circle1 != circle3, "Circle2D");
			UNIT_TEST_POSITIVE(circle1 != circle4, "Circle2D");
			UNIT_TEST_POSITIVE(circle1 != circle5, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle1(3.0f, Vector2D(2.0f, 3.0f));

			UNIT_TEST_POSITIVE(circle1.GetSquaredRadius() == 9.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;
			
			circle.SetCenter(Vector2D(2.0f, 3.0f));
			circle.SetRadius(1.0f);

			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 3.0f, "Circle2D");
			
			circle.Translate(Vector2D(1.0f, 1.0f));
			
			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 3.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 4.0f, "Circle2D");
			
			circle.Scale(0.5f);
			
			UNIT_TEST_POSITIVE(circle.GetRadius() == 0.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 3.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 4.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;

			circle = Circle2D::CreateFrom(1.0f, Vector2D(2.0f, 3.0f));
			
			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 3.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;

			circle = Circle2D::CreateFrom(Line2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 1.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;

			circle = Circle2D::CreateFrom(Capsule2D(1.0f, Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 1.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;

			circle = Circle2D::CreateFrom(AARect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.4142135f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 2.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;
			
			float a = Dia::Maths::Sin(Dia::Maths::Angle::Deg45) * 2.0f;
			circle = Circle2D::CreateFrom(Triangle2D(Vector2D(0.0f, 2.0f), Vector2D(a, -a), Vector2D(-a, -a)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;

			circle = Circle2D::CreateFrom(OORect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f), Vector2D(3.0f, 3.0f), Vector2D(1.0f, 3.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.4142135f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 2.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle;

			circle = Circle2D::CreateFrom( Arc2D( 2.0f, Angle::Deg30, Vector2D(1.0f, 1.0f), Vector2D(1.0f, 0.0f) ) );

			UNIT_TEST_POSITIVE(circle.GetRadius() == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 1.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 1.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude(Vector2D(2.0, 0.0f));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 2.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude(Circle2D(2.0f, Vector2D(2.0, 0.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 4.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude(Line2D(Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 3.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude(Capsule2D(2.0f, Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 5.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude(AARect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 4.2426405f), "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()
			
		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude(OORect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f), Vector2D(3.0f, 3.0f), Vector2D(1.0f, 3.0f)));

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 4.2426405f), "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude(Triangle2D(Vector2D(0.0f, 2.0f), Vector2D(2.0f, 2.0f), Vector2D(0.0f, 0.0f)));

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.8284271f), "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ExpandToInclude( Arc2D( 2.0f, Angle::Deg30, Vector2D(1.0f, 1.0f), Vector2D(1.0f, 0.0f) ) );

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 3.3013601f), "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.0f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()






		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude(Vector2D(2.0, 0.0f));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 1.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 0.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude(Circle2D(2.0f, Vector2D(2.0, 0.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 2.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == 1.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude(Line2D(Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 2.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == -0.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude(AARect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.6213205f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 1.1464466f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 1.1464466f), "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude(OORect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f), Vector2D(3.0f, 3.0f), Vector2D(1.0f, 3.0f)));

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.6879447f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 1.3584493f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 0.87153620f), "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude(Triangle2D(Vector2D(0.0f, 2.0f), Vector2D(2.0f, 2.0f), Vector2D(0.0f, 0.0f)));

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.0f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 0.39999998f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 0.79999995f), "Circle2D");

		UNIT_TEST_BLOCK_END()	

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude( Arc2D( 2.0f, Angle::Deg30, Vector2D(1.0f, 1.0f), Vector2D(1.0f, 0.0f) ) );

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.1858206f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 1.0910798f), "Circle2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 0.33892244f), "Circle2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Circle2D circle = Circle2D::UnitCircle();

			circle.ReContructToInclude(Capsule2D(2.0f, Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

			UNIT_TEST_POSITIVE(circle.GetRadius() == 4.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().x == -0.5f, "Circle2D");
			UNIT_TEST_POSITIVE(circle.GetCenter().y == 0.0f, "Circle2D");

		UNIT_TEST_BLOCK_END()

/*		void CalculateAxisPointToPoint( const Vector2D& pointOutsideCircle, Vector2D& closestPt, Vector2D& furthestPt ) const;
		void CalculateTangents( const Vector2D& pointOutsideCircle, Vector2D& tangent1, Vector2D& tangent2 ) const;

		void ClosestPointOnCircleTo(const Vector2D& point, Vector2D& result)const;
		void ClosestPointOnCircleTo(const AARect2D& rect, Vector2D& result)const;
		void ClosestPointOnCircleTo(const Arc2D& arc, Vector2D& result)const;
		void ClosestPointOnCircleTo(const Capsule2D& capsule, Vector2D& result)const;
		void ClosestPointOnCircleTo(const Circle2D& circle, Vector2D& result)const;
		void ClosestPointOnCircleTo(const Line2D& line, Vector2D& result)const;
		void ClosestPointOnCircleTo(const OORect2D& rect, Vector2D& result)const;
		void ClosestPointOnCircleTo(const Triangle2D& tri, Vector2D& result)const;

		IntersectionClassify IsIntersecting(const Vector2D& point)const;
		IntersectionClassify IsIntersecting(const AARect2D& rect)const;
		IntersectionClassify IsIntersecting(const Arc2D& arc)const;
		IntersectionClassify IsIntersecting(const Capsule2D& capsule)const;
		IntersectionClassify IsIntersecting(const Circle2D& circle)const;
		IntersectionClassify IsIntersecting(const Line2D& line)const;
		IntersectionClassify IsIntersecting(const OORect2D& rhs)const;
		IntersectionClassify IsIntersecting(const Triangle2D& rhs)const;

		//int CalculateIntercepts(const AARect2D& rect, Vector2D& intercept1, Vector2D& intercept1)const;
		//int CalculateIntercepts(const Arc2D& arc, Vector2D& intercept1, Vector2D& intercept1)const;
		//int CalculateIntercepts(const Capsule2D& capsule, Vector2D& intercept1, Vector2D& intercept1)const;
		int CalculateIntercepts(const Circle2D& circle, Vector2D& intercept1, Vector2D& intercept2)const;
		//			int CalculateIntercepts(const Line2D& rhs, Vector2D& intercept1, Vector2D& intercept2)const;
		//int CalculateIntercepts(const OORect2D& rhs, Vector2D& intercept1, Vector2D& intercept1)const;
		//int CalculateIntercepts(const Triangle2D& rhs, Vector2D& intercept1, Vector2D& intercept1)const;*/
		mState = kFinished;
	}
}
