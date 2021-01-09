
#include "UnitTests/Tests/Maths/UnitTestHalfVector2D.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"
#include <DiaCore/Strings/String128.h>
#include <DiaMaths/Vector/VectorHalf2D.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Matrix/Matrix22.h>

using Dia::Maths::VectorHalf2D;
using Dia::Maths::Matrix22;
using Dia::Maths::Angle;

namespace UnitTests
{	
	UnitTestVectorHalf2D::UnitTestVectorHalf2D(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestVectorHalf2D::UnitTestVectorHalf2D(void)
		: UnitTestMaths()
	{}

	void UnitTestVectorHalf2D::DoTest()
	{
		UNIT_TEST_BLOCK_START()
		
			VectorHalf2D vector1;
			VectorHalf2D vector2(1.0f, 2.0f);
			VectorHalf2D vector3(3.0f);
			VectorHalf2D vector4(vector3);		
			VectorHalf2D vector5 = vector3;
		
			UNIT_TEST_POSITIVE(vector1.x == 0.0f && vector1.y == 0.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.x == 3.0f && vector3.y == 3.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.x == 3.0f && vector4.y == 3.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.x == 3.0f && vector5.y == 3.0f, "VectorHalf2D");
	
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
		
			UNIT_TEST_POSITIVE(VectorHalf2D::XAxis().x == 1.0f && VectorHalf2D::XAxis().y == 0.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(VectorHalf2D::YAxis().x == 0.0f && VectorHalf2D::YAxis().y == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(VectorHalf2D::Zero().x == 0.0f && VectorHalf2D::Zero().y == 0.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(VectorHalf2D::Max().x == Dia::Maths::HalfFloat::Max() && VectorHalf2D::Max().y == Dia::Maths::HalfFloat::Max(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(VectorHalf2D::Min().x == Dia::Maths::HalfFloat::Min() && VectorHalf2D::Min().y == Dia::Maths::HalfFloat::Min(), "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1;
			vector1[0] = 1.0f;
			vector1[1] = 2.0f;

			UNIT_TEST_ASSERT_EXPECTED_START();
			vector1[-1] = 3.0f; 
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			vector1[2] = 3.0f; 
			UNIT_TEST_ASSERT_EXPECTED_END();			
	
			float a = vector1.x.ToFloat();
			float b = vector1.y.ToFloat();

			UNIT_TEST_POSITIVE(vector1.x == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.x == vector1[0], "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.y == vector1[1], "VectorHalf2D");
			
			VectorHalf2D vector2;
			vector2.X(1.0f);
			vector2.Y(2.0f);

			UNIT_TEST_POSITIVE(vector2.x == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.x == vector2.X(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.y == vector2.Y(), "VectorHalf2D");
			
			VectorHalf2D vector3;
			vector3.Set(1.0f, 2.0f);
			
			UNIT_TEST_POSITIVE(vector3.x == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.y == 2.0f, "VectorHalf2D");
			
			VectorHalf2D vector4;
			vector4.Set(vector3);

			UNIT_TEST_POSITIVE(vector4.x == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.y == 2.0f, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 2.0f);
			VectorHalf2D vector2 = -vector1;
			VectorHalf2D vector3(1.0f, 2.0f);
			VectorHalf2D vector4(2.0f, 3.0f);

			UNIT_TEST_POSITIVE(vector2.x == -1.0f && vector2.y == -2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1 == vector3, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1 != vector4, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 2.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(2.0f, 3.0f);

			vector2 += vector1;

			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.x == 2.0f && vector2.y == 3.0f, "VectorHalf2D");

			vector2 -= vector1;
			
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 1.0f, "VectorHalf2D");

			vector3 *= vector1;
			
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 6.0f, "VectorHalf2D");

			vector3 *= vector1;
		
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 12.0f, "VectorHalf2D");

			vector3 *= 1.5f;
			
			UNIT_TEST_POSITIVE(vector3.x == 3.0f && vector3.y == 18.0f, "VectorHalf2D");

			vector3 /= 1.5f;
			
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 12.0f, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			VectorHalf2D vector1(1.0f, 2.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(2.0f, 2.0f);

			VectorHalf2D vector4 = vector1 + vector2;

			UNIT_TEST_POSITIVE(vector4.x == 2.0f && vector4.y == 3.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 1.0f, "VectorHalf2D");

			VectorHalf2D vector5 = vector1 - vector2;

			UNIT_TEST_POSITIVE(vector5.x == 0.0f && vector5.y == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 1.0f, "VectorHalf2D");

			VectorHalf2D vector6 = vector1 * vector3;

			UNIT_TEST_POSITIVE(vector6.x == 2.0f && vector6.y == 4.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 2.0f, "VectorHalf2D");

			VectorHalf2D vector7 = vector1 / vector3;

			UNIT_TEST_POSITIVE(vector7.x == 0.5f && vector7.y == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 2.0f, "VectorHalf2D");

			VectorHalf2D vector8 = vector1 * 2.0f;

			UNIT_TEST_POSITIVE(vector8.x == 2.0f && vector8.y == 4.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			
			VectorHalf2D vector9 = vector1 / 2.0f;

			UNIT_TEST_POSITIVE(vector9.x == 0.5f && vector9.y == 1.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 2.0f);
			VectorHalf2D vector2 = vector1.AsInverse();
			
			UNIT_TEST_POSITIVE(vector2.x == -1.0f && vector2.y == -2.0f, "VectorHalf2D");

			vector1.Invert();
			
			UNIT_TEST_POSITIVE(vector1.x == -1.0f && vector1.y == -2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsValid(), "VectorHalf2D");
			
			vector1.Clear();
			
			UNIT_TEST_POSITIVE(vector1.x == 0.0f && vector1.y == 0.0f, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 2.0f);
			VectorHalf2D vector2 = vector1.Absolute();
			vector1.Absolutize();

			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 2.0f, "VectorHalf2D");

			VectorHalf2D vector3(-1.0f, -2.0f);
			VectorHalf2D vector4 = vector3.Absolute();
			vector3.Absolutize();

			UNIT_TEST_POSITIVE(vector3.x == 1.0f && vector3.y == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.x == 1.0f && vector4.y == 2.0f, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			VectorHalf2D vector1(1.0f, 2.0f);
			VectorHalf2D vector2(2.0f, 3.0f);
			
			float dot = vector1.Dot(vector2);
			float sqMag = vector1.SquareMagnitude();
			float mag = vector1.Magnitude();
			float distTo = vector1.DistanceTo(vector2);
			float sqDistTo = vector1.SquareDistanceTo(vector2);
			float manhattenDistance = vector1.ManhattanDistanceTo(vector2);

			UNIT_TEST_POSITIVE(dot == 8.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 1.41421356f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqDistTo == 2.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 2.0f, "VectorHalf2D");

			VectorHalf2D vector3(-1.0f, -2.0f);
			VectorHalf2D vector4(2.0f, 3.0f);
			
			dot = vector3.Dot(vector4);
			sqMag = vector3.SquareMagnitude();
			mag = vector3.Magnitude();
			distTo = vector3.DistanceTo(vector4);
			sqDistTo = vector3.SquareDistanceTo(vector4);
			manhattenDistance = vector3.ManhattanDistanceTo(vector4);

			UNIT_TEST_POSITIVE(dot == -8.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 5.83095189f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqDistTo == 34.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 8.0f, "VectorHalf2D");

			VectorHalf2D vector5(1.0f, 2.0f);
			VectorHalf2D vector6(-2.0f, -3.0f);

			dot = vector5.Dot(vector6);
			sqMag = vector5.SquareMagnitude();
			mag = vector5.Magnitude();
			distTo = vector5.DistanceTo(vector6);
			sqDistTo = vector5.SquareDistanceTo(vector6);
			manhattenDistance = vector1.ManhattanDistanceTo(vector6);

			UNIT_TEST_POSITIVE(dot == -8.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 5.83095189f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqDistTo == 34.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 8.0f, "VectorHalf2D");

			VectorHalf2D vector7(1.0f, 2.0f);
			VectorHalf2D vector8(1.0f, 2.0f);
			
			dot = vector7.Dot(vector8);
			sqMag = vector7.SquareMagnitude();
			mag = vector7.Magnitude();
			distTo = vector7.DistanceTo(vector8);
			sqDistTo = vector7.SquareDistanceTo(vector8);
			manhattenDistance = vector7.ManhattanDistanceTo(vector8);
					
			UNIT_TEST_POSITIVE(dot == 5.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqDistTo == 0.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 0.0f, "VectorHalf2D");

			VectorHalf2D vector9(-1.0f, -2.0f);
			VectorHalf2D vector10(-1.0f, -2.0f);
			
			dot = vector9.Dot(vector10);
			sqMag = vector9.SquareMagnitude();
			mag = vector9.Magnitude();
			distTo = vector9.DistanceTo(vector10);
			sqDistTo = vector9.SquareDistanceTo(vector10);
			manhattenDistance = vector9.ManhattanDistanceTo(vector10);

			UNIT_TEST_POSITIVE(dot == 5.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(sqDistTo == 0.0f, "VectorHalf2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 0.0f, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 0.0f);
			VectorHalf2D vector2(0.707106f, 0.707106f);
			VectorHalf2D vector3(0.0f, -1.0f);
			VectorHalf2D vector4(-0.707106f, -0.707106f);
			
			UNIT_TEST_POSITIVE(vector1.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsNormal(), "VectorHalf2D");
			
			VectorHalf2D vector5(1.0f, 0.1f);
			VectorHalf2D vector6(0.5f, 0.5f);
			VectorHalf2D vector7(-0.1f, -1.0f);
			VectorHalf2D vector8(-0.5555f, -0.55555f);
			
			UNIT_TEST_POSITIVE(!vector5.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(!vector6.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(!vector7.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(!vector8.IsNormal(), "VectorHalf2D");
			
			VectorHalf2D vector9 = vector5.AsNormal();
			VectorHalf2D vector10 = vector6.AsNormal();
			VectorHalf2D vector11 = vector7.AsNormal();
			VectorHalf2D vector12 = vector8.AsNormal();
			
			UNIT_TEST_POSITIVE(vector9.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector10.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector11.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector12.IsNormal(), "VectorHalf2D");
			
			vector5.Normalize();
			vector6.Normalize();
			vector7.Normalize();
			vector8.Normalize();
				
			UNIT_TEST_POSITIVE(vector5.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsNormal(), "VectorHalf2D");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			VectorHalf2D vector13(0.0f, 0.0f);
			VectorHalf2D vector14 = vector13.AsNormal();		
			UNIT_TEST_ASSERT_EXPECTED_END();	

			UNIT_TEST_ASSERT_EXPECTED_START();
			VectorHalf2D vector15(0.0f, 0.0f);
			vector15.Normalize();		
			UNIT_TEST_ASSERT_EXPECTED_END();
				
			VectorHalf2D vector16(1.0f, 0.1f);
			VectorHalf2D vector17(0.5111f, 0.5f);
			VectorHalf2D vector18(-0.1f, -1.0f);
			VectorHalf2D vector19(-0.5555f, -0.55555f);
			
			UNIT_TEST_POSITIVE(!vector16.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(!vector17.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(!vector18.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(!vector19.IsNormal(), "VectorHalf2D");
			
			VectorHalf2D vector20 = vector16.AsNormal();
			VectorHalf2D vector21 = vector17.AsNormal();
			VectorHalf2D vector22 = vector18.AsNormal();
			VectorHalf2D vector23 = vector19.AsNormal();
			
			UNIT_TEST_POSITIVE(vector20.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector21.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector22.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector23.IsNormal(), "VectorHalf2D");
			
			vector16.Normalize();
			vector17.Normalize();
			vector18.Normalize();
			vector19.Normalize();
				
			UNIT_TEST_POSITIVE(vector16.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector17.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector18.IsNormal(), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector19.IsNormal(), "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 2.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(2.0f, 1.0f);
			VectorHalf2D vector4(0.0f, 0.0f);

			UNIT_TEST_POSITIVE(vector1.AsSwitchedAxis() == VectorHalf2D(2.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.AsSwitchedAxis() == VectorHalf2D(1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.AsSwitchedAxis() == VectorHalf2D(1.0f, 2.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.AsSwitchedAxis() == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector1.SwitchAxis() == VectorHalf2D(2.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.SwitchAxis() == VectorHalf2D(1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.SwitchAxis() == VectorHalf2D(1.0f, 2.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.SwitchAxis() == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			VectorHalf2D vectorOnTo1(1.0f, 0.0f);
			VectorHalf2D vectorOnTo2(0.0f, 1.0f);
			VectorHalf2D vectorOnTo3(1.0f, 1.0f);
			VectorHalf2D vectorOnTo4(-1.0f, 1.0f);

			{
				VectorHalf2D vector1(0.5f, 0.0f);
				VectorHalf2D vector2(0.5f, 0.5f);
				VectorHalf2D vector3(0.0f, 0.5f);
				VectorHalf2D vector4(-0.5f, 0.5f);
				VectorHalf2D vector5(-0.5f, 0.0f);
				VectorHalf2D vector6(-0.5f, -0.5f);
				VectorHalf2D vector7(0.0f, -0.5f);
				VectorHalf2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(-0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(-0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(-0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f), "VectorHalf2D");
				
				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, -0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, -0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, -0.5f), "VectorHalf2D");

				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(0.5f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(-0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(-0.5f, -0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(-0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo3) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");

				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(-0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(-0.5f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(-0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo4) == VectorHalf2D(0.5f, -0.5f), "VectorHalf2D");
			}
			{
				VectorHalf2D vector1(0.5f, 0.0f);
				VectorHalf2D vector2(0.5f, 0.5f);
				VectorHalf2D vector3(0.0f, 0.5f);
				VectorHalf2D vector4(-0.5f, 0.5f);
				VectorHalf2D vector5(-0.5f, 0.0f);
				VectorHalf2D vector6(-0.5f, -0.5f);
				VectorHalf2D vector7(0.0f, -0.5f);
				VectorHalf2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo1) == VectorHalf2D(-0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo1) == VectorHalf2D(-0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo1) == VectorHalf2D(-0.5f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo1) == VectorHalf2D(0.5f, 0.0f), "VectorHalf2D");
				
			}
			{
				VectorHalf2D vector1(0.5f, 0.0f);
				VectorHalf2D vector2(0.5f, 0.5f);
				VectorHalf2D vector3(0.0f, 0.5f);
				VectorHalf2D vector4(-0.5f, 0.5f);
				VectorHalf2D vector5(-0.5f, 0.0f);
				VectorHalf2D vector6(-0.5f, -0.5f);
				VectorHalf2D vector7(0.0f, -0.5f);
				VectorHalf2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, -0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, -0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo2) == VectorHalf2D(0.0f, -0.5f), "VectorHalf2D");
			}

			{
				VectorHalf2D vector1(0.5f, 0.0f);
				VectorHalf2D vector2(0.5f, 0.5f);
				VectorHalf2D vector3(0.0f, 0.5f);
				VectorHalf2D vector4(-0.5f, 0.5f);
				VectorHalf2D vector5(-0.5f, 0.0f);
				VectorHalf2D vector6(-0.5f, -0.5f);
				VectorHalf2D vector7(0.0f, -0.5f);
				VectorHalf2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo3) == VectorHalf2D(0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo3) == VectorHalf2D(0.5f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo3) == VectorHalf2D(0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo3) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo3) == VectorHalf2D(-0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo3) == VectorHalf2D(-0.5f, -0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo3) == VectorHalf2D(-0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo3) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
			}

			{
				VectorHalf2D vector1(0.5f, 0.0f);
				VectorHalf2D vector2(0.5f, 0.5f);
				VectorHalf2D vector3(0.0f, 0.5f);
				VectorHalf2D vector4(-0.5f, 0.5f);
				VectorHalf2D vector5(-0.5f, 0.0f);
				VectorHalf2D vector6(-0.5f, -0.5f);
				VectorHalf2D vector7(0.0f, -0.5f);
				VectorHalf2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo4) == VectorHalf2D(0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo4) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo4) == VectorHalf2D(-0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo4) == VectorHalf2D(-0.5f, 0.5f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo4) == VectorHalf2D(-0.25f, 0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo4) == VectorHalf2D(0.0f, 0.0f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo4) == VectorHalf2D(0.25f, -0.25f), "VectorHalf2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo4) == VectorHalf2D(0.5f, -0.5f), "VectorHalf2D");
			}

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 0.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(0.0f, 1.0f);
			VectorHalf2D vector4(-1.0f, 1.0f);
			VectorHalf2D vector5(-1.0f, 0.0f);
			VectorHalf2D vector6(-1.0f, -1.0f);
			VectorHalf2D vector7(0.0f, -1.0f);
			VectorHalf2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.AsRotated90DegreeCounterClockwise() == VectorHalf2D(0.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.AsRotated90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.AsRotated90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.AsRotated90DegreeCounterClockwise() == VectorHalf2D(-1.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.AsRotated90DegreeCounterClockwise() == VectorHalf2D(0.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.AsRotated90DegreeCounterClockwise() == VectorHalf2D(1.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.AsRotated90DegreeCounterClockwise() == VectorHalf2D(1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.AsRotated90DegreeCounterClockwise() == VectorHalf2D(1.0f, 1.0f), "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector1.Rotate90DegreeCounterClockwise() == VectorHalf2D(0.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.Rotate90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.Rotate90DegreeCounterClockwise() == VectorHalf2D(-1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.Rotate90DegreeCounterClockwise() == VectorHalf2D(-1.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.Rotate90DegreeCounterClockwise() == VectorHalf2D(0.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.Rotate90DegreeCounterClockwise() == VectorHalf2D(1.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.Rotate90DegreeCounterClockwise() == VectorHalf2D(1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.Rotate90DegreeCounterClockwise() == VectorHalf2D(1.0f, 1.0f), "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 0.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(0.0f, 1.0f);
			VectorHalf2D vector4(-1.0f, 1.0f);
			VectorHalf2D vector5(-1.0f, 0.0f);
			VectorHalf2D vector6(-1.0f, -1.0f);
			VectorHalf2D vector7(0.0f, -1.0f);
			VectorHalf2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.AsRotated90DegreeClockwise() == VectorHalf2D(0.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.AsRotated90DegreeClockwise() == VectorHalf2D(1.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.AsRotated90DegreeClockwise() == VectorHalf2D(1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.AsRotated90DegreeClockwise() == VectorHalf2D(1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.AsRotated90DegreeClockwise() == VectorHalf2D(0.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.AsRotated90DegreeClockwise() == VectorHalf2D(-1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.AsRotated90DegreeClockwise() == VectorHalf2D(-1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.AsRotated90DegreeClockwise() == VectorHalf2D(-1.0f, -1.0f), "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector1.Rotate90DegreeClockwise() == VectorHalf2D(0.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.Rotate90DegreeClockwise() == VectorHalf2D(1.0f, -1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.Rotate90DegreeClockwise() == VectorHalf2D(1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.Rotate90DegreeClockwise() == VectorHalf2D(1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.Rotate90DegreeClockwise() == VectorHalf2D(0.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.Rotate90DegreeClockwise() == VectorHalf2D(-1.0f, 1.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.Rotate90DegreeClockwise() == VectorHalf2D(-1.0f, 0.0f), "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.Rotate90DegreeClockwise() == VectorHalf2D(-1.0f, -1.0f), "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 0.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(0.0f, 1.0f);
			VectorHalf2D vector4(-1.0f, 1.0f);
			VectorHalf2D vector5(-1.0f, 0.0f);
			VectorHalf2D vector6(-1.0f, -1.0f);
			VectorHalf2D vector7(0.0f, -1.0f);
			VectorHalf2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector8) == true, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector8) == true, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector8) == true, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector8) == false, "VectorHalf2D");
			
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector8) == false, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 0.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(0.0f, 1.0f);
			VectorHalf2D vector4(-1.0f, 1.0f);
			VectorHalf2D vector5(-1.0f, 0.0f);
			VectorHalf2D vector6(-1.0f, -1.0f);
			VectorHalf2D vector7(0.0f, -1.0f);
			VectorHalf2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector8) == true, "VectorHalf2D");
			
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector8) == true, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector8) == true, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector8) == false, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			VectorHalf2D vector1(1.0f, 0.0f);
			VectorHalf2D vector2(1.0f, 1.0f);
			VectorHalf2D vector3(0.0f, 1.0f);
			VectorHalf2D vector4(-1.0f, 1.0f);
			VectorHalf2D vector5(-1.0f, 0.0f);
			VectorHalf2D vector6(-1.0f, -1.0f);
			VectorHalf2D vector7(0.0f, -1.0f);
			VectorHalf2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector8) == true, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector1) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector5) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector8) == false, "VectorHalf2D");
			
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector2) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector6) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector3) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector4) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector7) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector8) == false, "VectorHalf2D");

			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector1) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector2) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector3) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector4) == true, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector5) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector6) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector7) == false, "VectorHalf2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector8) == true, "VectorHalf2D");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
