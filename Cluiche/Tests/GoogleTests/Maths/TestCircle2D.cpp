#include <gtest/gtest.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Shape/2D/Circle2D.h>
#include <DiaMaths/Shape/2D/Line2D.h>
#include <DiaMaths/Shape/2D/Capsule2D.h>
#include <DiaMaths/Shape/2D/AARect2D.h>
#include <DiaMaths/Shape/2D/OORect2D.h>
#include <DiaMaths/Shape/2D/Triangle2D.h>
#include <DiaMaths/Shape/2D/Arc2D.h>

using namespace Dia::Maths;

TEST(Circle2D, DefaultConstruction_IsAtOriginWithZeroRadius)
{
	Circle2D circle;

	EXPECT_EQ(circle.GetRadius(), 0.0f);
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ConstructWithParameters_SetsRadiusAndCenter)
{
	Circle2D circle(1.0f, Vector2D(2.0f, 3.0f));

	EXPECT_EQ(circle.GetRadius(), 1.0f);
	EXPECT_EQ(circle.GetCenter().x, 2.0f);
	EXPECT_EQ(circle.GetCenter().y, 3.0f);
}

TEST(Circle2D, CopyConstructor_CopiesRadiusAndCenter)
{
	Circle2D circle1(1.0f, Vector2D(2.0f, 3.0f));
	Circle2D circle2(circle1);

	EXPECT_EQ(circle2.GetRadius(), 1.0f);
	EXPECT_EQ(circle2.GetCenter().x, 2.0f);
	EXPECT_EQ(circle2.GetCenter().y, 3.0f);
}

TEST(Circle2D, AssignmentOperator_CopiesRadiusAndCenter)
{
	Circle2D circle1(1.0f, Vector2D(2.0f, 3.0f));
	Circle2D circle2 = circle1;

	EXPECT_EQ(circle2.GetRadius(), 1.0f);
	EXPECT_EQ(circle2.GetCenter().x, 2.0f);
	EXPECT_EQ(circle2.GetCenter().y, 3.0f);
}

TEST(Circle2D, EqualityOperators_CompareRadiusAndCenter)
{
	Circle2D circle1(1.0f, Vector2D(2.0f, 3.0f));
	Circle2D circle2 = circle1;
	Circle2D circle3(2.0f, Vector2D(2.0f, 3.0f));
	Circle2D circle4(1.0f, Vector2D(1.0f, 3.0f));
	Circle2D circle5(1.0f, Vector2D(2.0f, 1.0f));

	EXPECT_TRUE(circle1 == circle2);
	EXPECT_TRUE(circle1 != circle3);
	EXPECT_TRUE(circle1 != circle4);
	EXPECT_TRUE(circle1 != circle5);
}

TEST(Circle2D, GetSquaredRadius_ReturnsRadiusSquared)
{
	Circle2D circle1(3.0f, Vector2D(2.0f, 3.0f));

	EXPECT_EQ(circle1.GetSquaredRadius(), 9.0f);
}

TEST(Circle2D, SettersAndTransform_ModifyCircle)
{
	Circle2D circle;

	circle.SetCenter(Vector2D(2.0f, 3.0f));
	circle.SetRadius(1.0f);

	EXPECT_EQ(circle.GetRadius(), 1.0f);
	EXPECT_EQ(circle.GetCenter().x, 2.0f);
	EXPECT_EQ(circle.GetCenter().y, 3.0f);

	circle.Translate(Vector2D(1.0f, 1.0f));

	EXPECT_EQ(circle.GetRadius(), 1.0f);
	EXPECT_EQ(circle.GetCenter().x, 3.0f);
	EXPECT_EQ(circle.GetCenter().y, 4.0f);

	circle.Scale(0.5f);

	EXPECT_EQ(circle.GetRadius(), 0.5f);
	EXPECT_EQ(circle.GetCenter().x, 3.0f);
	EXPECT_EQ(circle.GetCenter().y, 4.0f);
}

TEST(Circle2D, CreateFromRadiusAndCenter_CreatesCircle)
{
	Circle2D circle;

	circle = Circle2D::CreateFrom(1.0f, Vector2D(2.0f, 3.0f));

	EXPECT_EQ(circle.GetRadius(), 1.0f);
	EXPECT_EQ(circle.GetCenter().x, 2.0f);
	EXPECT_EQ(circle.GetCenter().y, 3.0f);
}

TEST(Circle2D, CreateFromLine2D_CreatesBoundingCircle)
{
	Circle2D circle;

	circle = Circle2D::CreateFrom(Line2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f)));

	EXPECT_EQ(circle.GetRadius(), 1.0f);
	EXPECT_EQ(circle.GetCenter().x, 2.0f);
	EXPECT_EQ(circle.GetCenter().y, 1.0f);
}

TEST(Circle2D, CreateFromCapsule2D_CreatesBoundingCircle)
{
	Circle2D circle;

	circle = Circle2D::CreateFrom(Capsule2D(1.0f, Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f)));

	EXPECT_EQ(circle.GetRadius(), 2.0f);
	EXPECT_EQ(circle.GetCenter().x, 2.0f);
	EXPECT_EQ(circle.GetCenter().y, 1.0f);
}

TEST(Circle2D, CreateFromAARect2D_CreatesBoundingCircle)
{
	Circle2D circle;

	circle = Circle2D::CreateFrom(AARect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

	EXPECT_EQ(circle.GetRadius(), 1.4142135f);
	EXPECT_EQ(circle.GetCenter().x, 2.0f);
	EXPECT_EQ(circle.GetCenter().y, 2.0f);
}

TEST(Circle2D, CreateFromTriangle2D_CreatesCircumscribingCircle)
{
	Circle2D circle;

	float a = Dia::Maths::Sin(Dia::Maths::Angle::Deg45) * 2.0f;
	circle = Circle2D::CreateFrom(Triangle2D(Vector2D(0.0f, 2.0f), Vector2D(a, -a), Vector2D(-a, -a)));

	EXPECT_EQ(circle.GetRadius(), 2.0f);
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, CreateFromOORect2D_CreatesBoundingCircle)
{
	Circle2D circle;

	circle = Circle2D::CreateFrom(OORect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f), Vector2D(3.0f, 3.0f), Vector2D(1.0f, 3.0f)));

	EXPECT_EQ(circle.GetRadius(), 1.4142135f);
	EXPECT_EQ(circle.GetCenter().x, 2.0f);
	EXPECT_EQ(circle.GetCenter().y, 2.0f);
}

TEST(Circle2D, CreateFromArc2D_CreatesBoundingCircle)
{
	Circle2D circle;

	circle = Circle2D::CreateFrom(Arc2D(2.0f, Angle::Deg30, Vector2D(1.0f, 1.0f), Vector2D(1.0f, 0.0f)));

	EXPECT_EQ(circle.GetRadius(), 2.0f);
	EXPECT_EQ(circle.GetCenter().x, 1.0f);
	EXPECT_EQ(circle.GetCenter().y, 1.0f);
}

TEST(Circle2D, ExpandToIncludeVector2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(Vector2D(2.0, 0.0f));

	EXPECT_EQ(circle.GetRadius(), 2.0f);
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ExpandToIncludeCircle2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(Circle2D(2.0f, Vector2D(2.0, 0.0f)));

	EXPECT_EQ(circle.GetRadius(), 4.0f);
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ExpandToIncludeLine2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(Line2D(Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

	EXPECT_EQ(circle.GetRadius(), 3.0f);
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ExpandToIncludeCapsule2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(Capsule2D(2.0f, Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

	EXPECT_EQ(circle.GetRadius(), 5.0f);
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ExpandToIncludeAARect2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(AARect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 4.2426405f));
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ExpandToIncludeOORect2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(OORect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f), Vector2D(3.0f, 3.0f), Vector2D(1.0f, 3.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 4.2426405f));
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ExpandToIncludeTriangle2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(Triangle2D(Vector2D(0.0f, 2.0f), Vector2D(2.0f, 2.0f), Vector2D(0.0f, 0.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.8284271f));
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ExpandToIncludeArc2D_ExpandsRadius)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ExpandToInclude(Arc2D(2.0f, Angle::Deg30, Vector2D(1.0f, 1.0f), Vector2D(1.0f, 0.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 3.3013601f));
	EXPECT_EQ(circle.GetCenter().x, 0.0f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ReContructToIncludeVector2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(Vector2D(2.0, 0.0f));

	EXPECT_EQ(circle.GetRadius(), 1.5f);
	EXPECT_EQ(circle.GetCenter().x, 0.5f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ReContructToIncludeCircle2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(Circle2D(2.0f, Vector2D(2.0, 0.0f)));

	EXPECT_EQ(circle.GetRadius(), 2.5f);
	EXPECT_EQ(circle.GetCenter().x, 1.5f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ReContructToIncludeLine2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(Line2D(Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

	EXPECT_EQ(circle.GetRadius(), 2.5f);
	EXPECT_EQ(circle.GetCenter().x, -0.5f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}

TEST(Circle2D, ReContructToIncludeAARect2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(AARect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 3.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.6213205f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 1.1464466f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 1.1464466f));
}

TEST(Circle2D, ReContructToIncludeOORect2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(OORect2D(Vector2D(1.0f, 1.0f), Vector2D(3.0f, 1.0f), Vector2D(3.0f, 3.0f), Vector2D(1.0f, 3.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.6879447f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 1.3584493f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 0.87153620f));
}

TEST(Circle2D, ReContructToIncludeTriangle2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(Triangle2D(Vector2D(0.0f, 2.0f), Vector2D(2.0f, 2.0f), Vector2D(0.0f, 0.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.0f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 0.39999998f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 0.79999995f));
}

TEST(Circle2D, ReContructToIncludeArc2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(Arc2D(2.0f, Angle::Deg30, Vector2D(1.0f, 1.0f), Vector2D(1.0f, 0.0f)));

	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetRadius(), 2.1858206f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().x, 1.0910798f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(circle.GetCenter().y, 0.33892244f));
}

TEST(Circle2D, ReContructToIncludeCapsule2D_RecentersCircle)
{
	Circle2D circle = Circle2D::UnitCircle();

	circle.ReContructToInclude(Capsule2D(2.0f, Vector2D(-3.0, 0.0f), Vector2D(2.0, 0.0f)));

	EXPECT_EQ(circle.GetRadius(), 4.5f);
	EXPECT_EQ(circle.GetCenter().x, -0.5f);
	EXPECT_EQ(circle.GetCenter().y, 0.0f);
}
