#include <gtest/gtest.h>
#include <DiaCore/Strings/String128.h>
#include <DiaMaths/Vector/VectorHalf2D.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Matrix/Matrix22.h>

using namespace Dia::Maths;

TEST(HalfVector2D, Construction_InitializesCorrectly)
{
	VectorHalf2D vector1;
	VectorHalf2D vector2(1.0f, 2.0f);
	VectorHalf2D vector3(3.0f);
	VectorHalf2D vector4(vector3);
	VectorHalf2D vector5 = vector3;

	EXPECT_EQ(vector1.x, 0.0f);
	EXPECT_EQ(vector1.y, 0.0f);
	EXPECT_EQ(vector2.x, 1.0f);
	EXPECT_EQ(vector2.y, 2.0f);
	EXPECT_EQ(vector3.x, 3.0f);
	EXPECT_EQ(vector3.y, 3.0f);
	EXPECT_EQ(vector4.x, 3.0f);
	EXPECT_EQ(vector4.y, 3.0f);
	EXPECT_EQ(vector5.x, 3.0f);
	EXPECT_EQ(vector5.y, 3.0f);
}

TEST(HalfVector2D, StaticMethods_ReturnCorrectVectors)
{
	EXPECT_EQ(VectorHalf2D::XAxis().x, 1.0f);
	EXPECT_EQ(VectorHalf2D::XAxis().y, 0.0f);
	EXPECT_EQ(VectorHalf2D::YAxis().x, 0.0f);
	EXPECT_EQ(VectorHalf2D::YAxis().y, 1.0f);
	EXPECT_EQ(VectorHalf2D::Zero().x, 0.0f);
	EXPECT_EQ(VectorHalf2D::Zero().y, 0.0f);
	EXPECT_EQ(VectorHalf2D::Max().x, HalfFloat::Max());
	EXPECT_EQ(VectorHalf2D::Max().y, HalfFloat::Max());
	EXPECT_EQ(VectorHalf2D::Min().x, HalfFloat::Min());
	EXPECT_EQ(VectorHalf2D::Min().y, HalfFloat::Min());
}

TEST(HalfVector2D, ElementAccess_WorksCorrectly)
{
	VectorHalf2D vector1;
	vector1[0] = 1.0f;
	vector1[1] = 2.0f;

	EXPECT_DEATH(vector1[-1] = 3.0f, "");
	EXPECT_DEATH(vector1[2] = 3.0f, "");

	float a = vector1.x.ToFloat();
	float b = vector1.y.ToFloat();

	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector1.x, vector1[0]);
	EXPECT_EQ(vector1.y, vector1[1]);

	VectorHalf2D vector2;
	vector2.X(1.0f);
	vector2.Y(2.0f);

	EXPECT_EQ(vector2.x, 1.0f);
	EXPECT_EQ(vector2.y, 2.0f);
	EXPECT_EQ(vector2.x, vector2.X());
	EXPECT_EQ(vector2.y, vector2.Y());

	VectorHalf2D vector3;
	vector3.Set(1.0f, 2.0f);

	EXPECT_EQ(vector3.x, 1.0f);
	EXPECT_EQ(vector3.y, 2.0f);

	VectorHalf2D vector4;
	vector4.Set(vector3);

	EXPECT_EQ(vector4.x, 1.0f);
	EXPECT_EQ(vector4.y, 2.0f);
}

TEST(HalfVector2D, UnaryNegationAndComparison_WorkCorrectly)
{
	VectorHalf2D vector1(1.0f, 2.0f);
	VectorHalf2D vector2 = -vector1;
	VectorHalf2D vector3(1.0f, 2.0f);
	VectorHalf2D vector4(2.0f, 3.0f);

	EXPECT_EQ(vector2.x, -1.0f);
	EXPECT_EQ(vector2.y, -2.0f);
	EXPECT_TRUE(vector1 == vector3);
	EXPECT_TRUE(vector1 != vector4);
}

TEST(HalfVector2D, CompoundOperators_ModifyVectorInPlace)
{
	VectorHalf2D vector1(1.0f, 2.0f);
	VectorHalf2D vector2(1.0f, 1.0f);
	VectorHalf2D vector3(2.0f, 3.0f);

	vector2 += vector1;

	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector2.x, 2.0f);
	EXPECT_EQ(vector2.y, 3.0f);

	vector2 -= vector1;

	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector2.x, 1.0f);
	EXPECT_EQ(vector2.y, 1.0f);

	vector3 *= vector1;

	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector3.x, 2.0f);
	EXPECT_EQ(vector3.y, 6.0f);

	vector3 *= vector1;

	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector3.x, 2.0f);
	EXPECT_EQ(vector3.y, 12.0f);

	vector3 *= 1.5f;

	EXPECT_EQ(vector3.x, 3.0f);
	EXPECT_EQ(vector3.y, 18.0f);

	vector3 /= 1.5f;

	EXPECT_EQ(vector3.x, 2.0f);
	EXPECT_EQ(vector3.y, 12.0f);
}

TEST(HalfVector2D, BinaryOperators_ReturnNewVectors)
{
	VectorHalf2D vector1(1.0f, 2.0f);
	VectorHalf2D vector2(1.0f, 1.0f);
	VectorHalf2D vector3(2.0f, 2.0f);

	VectorHalf2D vector4 = vector1 + vector2;

	EXPECT_EQ(vector4.x, 2.0f);
	EXPECT_EQ(vector4.y, 3.0f);
	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector2.x, 1.0f);
	EXPECT_EQ(vector2.y, 1.0f);

	VectorHalf2D vector5 = vector1 - vector2;

	EXPECT_EQ(vector5.x, 0.0f);
	EXPECT_EQ(vector5.y, 1.0f);
	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector2.x, 1.0f);
	EXPECT_EQ(vector2.y, 1.0f);

	VectorHalf2D vector6 = vector1 * vector3;

	EXPECT_EQ(vector6.x, 2.0f);
	EXPECT_EQ(vector6.y, 4.0f);
	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector3.x, 2.0f);
	EXPECT_EQ(vector3.y, 2.0f);

	VectorHalf2D vector7 = vector1 / vector3;

	EXPECT_EQ(vector7.x, 0.5f);
	EXPECT_EQ(vector7.y, 1.0f);
	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector3.x, 2.0f);
	EXPECT_EQ(vector3.y, 2.0f);

	VectorHalf2D vector8 = vector1 * 2.0f;

	EXPECT_EQ(vector8.x, 2.0f);
	EXPECT_EQ(vector8.y, 4.0f);
	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);

	VectorHalf2D vector9 = vector1 / 2.0f;

	EXPECT_EQ(vector9.x, 0.5f);
	EXPECT_EQ(vector9.y, 1.0f);
	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
}

TEST(HalfVector2D, InvertAndClear_ModifyVector)
{
	VectorHalf2D vector1(1.0f, 2.0f);
	VectorHalf2D vector2 = vector1.AsInverse();

	EXPECT_EQ(vector2.x, -1.0f);
	EXPECT_EQ(vector2.y, -2.0f);

	vector1.Invert();

	EXPECT_EQ(vector1.x, -1.0f);
	EXPECT_EQ(vector1.y, -2.0f);
	EXPECT_TRUE(vector1.IsValid());

	vector1.Clear();

	EXPECT_EQ(vector1.x, 0.0f);
	EXPECT_EQ(vector1.y, 0.0f);
}

TEST(HalfVector2D, Absolute_ReturnsAbsoluteValues)
{
	VectorHalf2D vector1(1.0f, 2.0f);
	VectorHalf2D vector2 = vector1.Absolute();
	vector1.Absolutize();

	EXPECT_EQ(vector1.x, 1.0f);
	EXPECT_EQ(vector1.y, 2.0f);
	EXPECT_EQ(vector2.x, 1.0f);
	EXPECT_EQ(vector2.y, 2.0f);

	VectorHalf2D vector3(-1.0f, -2.0f);
	VectorHalf2D vector4 = vector3.Absolute();
	vector3.Absolutize();

	EXPECT_EQ(vector3.x, 1.0f);
	EXPECT_EQ(vector3.y, 2.0f);
	EXPECT_EQ(vector4.x, 1.0f);
	EXPECT_EQ(vector4.y, 2.0f);
}

TEST(HalfVector2D, DotProductAndMagnitude_CalculateCorrectly)
{
	VectorHalf2D vector1(1.0f, 2.0f);
	VectorHalf2D vector2(2.0f, 3.0f);

	float dot = vector1.Dot(vector2);
	float sqMag = vector1.SquareMagnitude();
	float mag = vector1.Magnitude();
	float distTo = vector1.DistanceTo(vector2);
	float sqDistTo = vector1.SquareDistanceTo(vector2);
	float manhattenDistance = vector1.ManhattanDistanceTo(vector2);

	EXPECT_EQ(dot, 8.0f);
	EXPECT_EQ(sqMag, 5.0f);
	EXPECT_TRUE(Dia::Maths::Float::FEqual(mag, 2.23606798f));
	EXPECT_TRUE(Dia::Maths::Float::FEqual(distTo, 1.41421356f));
	EXPECT_EQ(sqDistTo, 2.0f);
	EXPECT_EQ(manhattenDistance, 2.0f);
}

TEST(HalfVector2D, Normalization_WorksCorrectly)
{
	VectorHalf2D vector1(1.0f, 0.0f);
	VectorHalf2D vector2(0.707106f, 0.707106f);
	VectorHalf2D vector3(0.0f, -1.0f);
	VectorHalf2D vector4(-0.707106f, -0.707106f);

	EXPECT_TRUE(vector1.IsNormal());
	EXPECT_TRUE(vector2.IsNormal());
	EXPECT_TRUE(vector3.IsNormal());
	EXPECT_TRUE(vector4.IsNormal());

	VectorHalf2D vector5(1.0f, 0.1f);
	VectorHalf2D vector6(0.5f, 0.5f);
	VectorHalf2D vector7(-0.1f, -1.0f);
	VectorHalf2D vector8(-0.5555f, -0.55555f);

	EXPECT_FALSE(vector5.IsNormal());
	EXPECT_FALSE(vector6.IsNormal());
	EXPECT_FALSE(vector7.IsNormal());
	EXPECT_FALSE(vector8.IsNormal());

	VectorHalf2D vector9 = vector5.AsNormal();
	VectorHalf2D vector10 = vector6.AsNormal();
	VectorHalf2D vector11 = vector7.AsNormal();
	VectorHalf2D vector12 = vector8.AsNormal();

	EXPECT_TRUE(vector9.IsNormal());
	EXPECT_TRUE(vector10.IsNormal());
	EXPECT_TRUE(vector11.IsNormal());
	EXPECT_TRUE(vector12.IsNormal());

	vector5.Normalize();
	vector6.Normalize();
	vector7.Normalize();
	vector8.Normalize();

	EXPECT_TRUE(vector5.IsNormal());
	EXPECT_TRUE(vector6.IsNormal());
	EXPECT_TRUE(vector7.IsNormal());
	EXPECT_TRUE(vector8.IsNormal());

	EXPECT_DEATH({ VectorHalf2D vector13(0.0f, 0.0f); VectorHalf2D vector14 = vector13.AsNormal(); }, "");
	EXPECT_DEATH({ VectorHalf2D vector15(0.0f, 0.0f); vector15.Normalize(); }, "");
}

TEST(HalfVector2D, SwitchAxis_SwapsXAndY)
{
	VectorHalf2D vector1(1.0f, 2.0f);
	VectorHalf2D vector2(1.0f, 1.0f);
	VectorHalf2D vector3(2.0f, 1.0f);
	VectorHalf2D vector4(0.0f, 0.0f);

	EXPECT_TRUE(vector1.AsSwitchedAxis() == VectorHalf2D(2.0f, 1.0f));
	EXPECT_TRUE(vector2.AsSwitchedAxis() == VectorHalf2D(1.0f, 1.0f));
	EXPECT_TRUE(vector3.AsSwitchedAxis() == VectorHalf2D(1.0f, 2.0f));
	EXPECT_TRUE(vector4.AsSwitchedAxis() == VectorHalf2D(0.0f, 0.0f));

	EXPECT_TRUE(vector1.SwitchAxis() == VectorHalf2D(2.0f, 1.0f));
	EXPECT_TRUE(vector2.SwitchAxis() == VectorHalf2D(1.0f, 1.0f));
	EXPECT_TRUE(vector3.SwitchAxis() == VectorHalf2D(1.0f, 2.0f));
	EXPECT_TRUE(vector4.SwitchAxis() == VectorHalf2D(0.0f, 0.0f));
}

TEST(HalfVector2D, Projection_WorksCorrectly)
{
	VectorHalf2D vectorOnTo1(1.0f, 0.0f);
	VectorHalf2D vector1(0.5f, 0.0f);
	VectorHalf2D vector2(0.5f, 0.5f);
	VectorHalf2D vector3(0.0f, 0.5f);

	EXPECT_TRUE(vector1.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f));
	EXPECT_TRUE(vector2.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f));
	EXPECT_TRUE(vector3.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.0f, 0.0f));

	EXPECT_TRUE(vector1.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f));
	EXPECT_TRUE(vector2.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f));
	EXPECT_TRUE(vector3.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.0f, 0.0f));
}

TEST(HalfVector2D, Rotation90Degrees_WorksCorrectly)
{
	VectorHalf2D vector1(1.0f, 0.0f);
	VectorHalf2D vector2(1.0f, 1.0f);
	VectorHalf2D vector3(0.0f, 1.0f);
	VectorHalf2D vector4(-1.0f, 1.0f);

	EXPECT_TRUE(vector1.AsRotated90DegreeCounterClockwise() == VectorHalf2D(0.0f, 1.0f));
	EXPECT_TRUE(vector2.AsRotated90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 1.0f));
	EXPECT_TRUE(vector3.AsRotated90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 0.0f));
	EXPECT_TRUE(vector4.AsRotated90DegreeCounterClockwise() == VectorHalf2D(-1.0f, -1.0f));

	EXPECT_TRUE(vector1.Rotate90DegreeCounterClockwise() == VectorHalf2D(0.0f, 1.0f));
	EXPECT_TRUE(vector2.Rotate90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 1.0f));
	EXPECT_TRUE(vector3.Rotate90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 0.0f));
	EXPECT_TRUE(vector4.Rotate90DegreeCounterClockwise() == VectorHalf2D(-1.0f, -1.0f));

	VectorHalf2D vector5(1.0f, 0.0f);
	VectorHalf2D vector6(1.0f, 1.0f);
	VectorHalf2D vector7(0.0f, 1.0f);
	VectorHalf2D vector8(-1.0f, 1.0f);

	EXPECT_TRUE(vector5.AsRotated90DegreeClockwise() == VectorHalf2D(0.0f, -1.0f));
	EXPECT_TRUE(vector6.AsRotated90DegreeClockwise() == VectorHalf2D(1.0f, -1.0f));
	EXPECT_TRUE(vector7.AsRotated90DegreeClockwise() == VectorHalf2D(1.0f, 0.0f));
	EXPECT_TRUE(vector8.AsRotated90DegreeClockwise() == VectorHalf2D(1.0f, 1.0f));

	EXPECT_TRUE(vector5.Rotate90DegreeClockwise() == VectorHalf2D(0.0f, -1.0f));
	EXPECT_TRUE(vector6.Rotate90DegreeClockwise() == VectorHalf2D(1.0f, -1.0f));
	EXPECT_TRUE(vector7.Rotate90DegreeClockwise() == VectorHalf2D(1.0f, 0.0f));
	EXPECT_TRUE(vector8.Rotate90DegreeClockwise() == VectorHalf2D(1.0f, 1.0f));
}

TEST(HalfVector2D, ClockwiseComparison_WorksCorrectly)
{
	VectorHalf2D vector1(1.0f, 0.0f);
	VectorHalf2D vector2(1.0f, 1.0f);
	VectorHalf2D vector3(0.0f, 1.0f);
	VectorHalf2D vector4(-1.0f, 1.0f);

	EXPECT_FALSE(vector1.IsClockwiseTo(vector1));
	EXPECT_FALSE(vector1.IsClockwiseTo(vector2));
	EXPECT_FALSE(vector1.IsClockwiseTo(vector3));
	EXPECT_FALSE(vector1.IsClockwiseTo(vector4));

	EXPECT_TRUE(vector2.IsClockwiseTo(vector1));
	EXPECT_FALSE(vector2.IsClockwiseTo(vector2));
	EXPECT_FALSE(vector2.IsClockwiseTo(vector3));
	EXPECT_FALSE(vector2.IsClockwiseTo(vector4));

	EXPECT_TRUE(vector3.IsClockwiseTo(vector1));
	EXPECT_TRUE(vector3.IsClockwiseTo(vector2));
	EXPECT_FALSE(vector3.IsClockwiseTo(vector3));
	EXPECT_FALSE(vector3.IsClockwiseTo(vector4));
}

TEST(HalfVector2D, CounterClockwiseComparison_WorksCorrectly)
{
	VectorHalf2D vector1(1.0f, 0.0f);
	VectorHalf2D vector2(1.0f, 1.0f);
	VectorHalf2D vector3(0.0f, 1.0f);
	VectorHalf2D vector4(-1.0f, 1.0f);

	EXPECT_FALSE(vector1.IsCounterClockwiseTo(vector1));
	EXPECT_TRUE(vector1.IsCounterClockwiseTo(vector2));
	EXPECT_TRUE(vector1.IsCounterClockwiseTo(vector3));
	EXPECT_TRUE(vector1.IsCounterClockwiseTo(vector4));

	EXPECT_FALSE(vector2.IsCounterClockwiseTo(vector1));
	EXPECT_FALSE(vector2.IsCounterClockwiseTo(vector2));
	EXPECT_TRUE(vector2.IsCounterClockwiseTo(vector3));
	EXPECT_TRUE(vector2.IsCounterClockwiseTo(vector4));

	EXPECT_FALSE(vector3.IsCounterClockwiseTo(vector1));
	EXPECT_FALSE(vector3.IsCounterClockwiseTo(vector2));
	EXPECT_FALSE(vector3.IsCounterClockwiseTo(vector3));
	EXPECT_TRUE(vector3.IsCounterClockwiseTo(vector4));
}

TEST(HalfVector2D, ParallelComparison_WorksCorrectly)
{
	VectorHalf2D vector1(1.0f, 0.0f);
	VectorHalf2D vector2(1.0f, 1.0f);
	VectorHalf2D vector3(0.0f, 1.0f);
	VectorHalf2D vector4(-1.0f, 1.0f);
	VectorHalf2D vector5(-1.0f, 0.0f);
	VectorHalf2D vector6(-1.0f, -1.0f);
	VectorHalf2D vector7(0.0f, -1.0f);
	VectorHalf2D vector8(1.0f, -1.0f);

	EXPECT_TRUE(vector1.IsParallelTo(vector1));
	EXPECT_FALSE(vector1.IsParallelTo(vector2));
	EXPECT_FALSE(vector1.IsParallelTo(vector3));
	EXPECT_FALSE(vector1.IsParallelTo(vector4));
	EXPECT_TRUE(vector1.IsParallelTo(vector5));
	EXPECT_FALSE(vector1.IsParallelTo(vector6));
	EXPECT_FALSE(vector1.IsParallelTo(vector7));
	EXPECT_FALSE(vector1.IsParallelTo(vector8));

	EXPECT_FALSE(vector2.IsParallelTo(vector1));
	EXPECT_TRUE(vector2.IsParallelTo(vector2));
	EXPECT_FALSE(vector2.IsParallelTo(vector3));
	EXPECT_FALSE(vector2.IsParallelTo(vector4));
	EXPECT_FALSE(vector2.IsParallelTo(vector5));
	EXPECT_TRUE(vector2.IsParallelTo(vector6));
	EXPECT_FALSE(vector2.IsParallelTo(vector7));
	EXPECT_FALSE(vector2.IsParallelTo(vector8));

	EXPECT_FALSE(vector3.IsParallelTo(vector1));
	EXPECT_FALSE(vector3.IsParallelTo(vector2));
	EXPECT_TRUE(vector3.IsParallelTo(vector3));
	EXPECT_FALSE(vector3.IsParallelTo(vector4));
	EXPECT_FALSE(vector3.IsParallelTo(vector5));
	EXPECT_FALSE(vector3.IsParallelTo(vector6));
	EXPECT_TRUE(vector3.IsParallelTo(vector7));
	EXPECT_FALSE(vector3.IsParallelTo(vector8));
}
