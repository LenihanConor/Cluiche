
#include "UnitTests/Tests/Maths/UnitTestVector2D.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"
#include <DiaCore/Strings/String128.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Matrix/Matrix22.h>

using Dia::Maths::Vector2D;
using Dia::Maths::Matrix22;
using Dia::Maths::Angle;

namespace UnitTests
{	
	UnitTestVector2D::UnitTestVector2D(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestVector2D::UnitTestVector2D(void)
		: UnitTestMaths()
	{}

	void UnitTestVector2D::DoTest()
	{
		UNIT_TEST_BLOCK_START()
		
			Vector2D vector1;
			Vector2D vector2(1.0f, 2.0f);
			Vector2D vector3(3.0f);
			Vector2D vector4(vector3);		
			Vector2D vector5 = vector3;
		
			UNIT_TEST_POSITIVE(vector1.x == 0.0f && vector1.y == 0.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.x == 3.0f && vector3.y == 3.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.x == 3.0f && vector4.y == 3.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.x == 3.0f && vector5.y == 3.0f, "Vector2D");
	
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
		
			UNIT_TEST_POSITIVE(Vector2D::XAxis().x == 1.0f && Vector2D::XAxis().y == 0.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Vector2D::YAxis().x == 0.0f && Vector2D::YAxis().y == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Vector2D::Zero().x == 0.0f && Vector2D::Zero().y == 0.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Vector2D::Max().x == Dia::Maths::Float::Max() && Vector2D::Max().y == Dia::Maths::Float::Max(), "Vector2D");
			UNIT_TEST_POSITIVE(Vector2D::Min().x == Dia::Maths::Float::Min() && Vector2D::Min().y == Dia::Maths::Float::Min(), "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1;
			vector1[0] = 1.0f;
			vector1[1] = 2.0f;

			UNIT_TEST_ASSERT_EXPECTED_START();
			vector1[-1] = 3.0f; 
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			vector1[2] = 3.0f; 
			UNIT_TEST_ASSERT_EXPECTED_END();			

			UNIT_TEST_POSITIVE(vector1.x == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.x == vector1[0], "Vector2D");
			UNIT_TEST_POSITIVE(vector1.y == vector1[1], "Vector2D");
			
			Vector2D vector2;
			vector2.X(1.0f);
			vector2.Y(2.0f);

			UNIT_TEST_POSITIVE(vector2.x == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.x == vector2.X(), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.y == vector2.Y(), "Vector2D");
			
			Vector2D vector3;
			vector3.Set(1.0f, 2.0f);
			
			UNIT_TEST_POSITIVE(vector3.x == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.y == 2.0f, "Vector2D");
			
			Vector2D vector4;
			vector4.Set(vector3);

			UNIT_TEST_POSITIVE(vector4.x == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.y == 2.0f, "Vector2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 2.0f);
			Vector2D vector2 = -vector1;
			Vector2D vector3(1.0f, 2.0f);
			Vector2D vector4(2.0f, 3.0f);

			UNIT_TEST_POSITIVE(vector2.x == -1.0f && vector2.y == -2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1 == vector3, "Vector2D");
			UNIT_TEST_POSITIVE(vector1 != vector4, "Vector2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 2.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(2.0f, 3.0f);

			vector2 += vector1;

			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.x == 2.0f && vector2.y == 3.0f, "Vector2D");

			vector2 -= vector1;
			
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 1.0f, "Vector2D");

			vector3 *= vector1;
			
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 6.0f, "Vector2D");

			vector3 *= vector1;
		
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 12.0f, "Vector2D");

			vector3 *= 1.5f;
			
			UNIT_TEST_POSITIVE(vector3.x == 3.0f && vector3.y == 18.0f, "Vector2D");

			vector3 /= 1.5f;
			
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 12.0f, "Vector2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Vector2D vector1(1.0f, 2.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(2.0f, 2.0f);

			Vector2D vector4 = vector1 + vector2;

			UNIT_TEST_POSITIVE(vector4.x == 2.0f && vector4.y == 3.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 1.0f, "Vector2D");

			Vector2D vector5 = vector1 - vector2;

			UNIT_TEST_POSITIVE(vector5.x == 0.0f && vector5.y == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 1.0f, "Vector2D");

			Vector2D vector6 = vector1 * vector3;

			UNIT_TEST_POSITIVE(vector6.x == 2.0f && vector6.y == 4.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 2.0f, "Vector2D");

			Vector2D vector7 = vector1 / vector3;

			UNIT_TEST_POSITIVE(vector7.x == 0.5f && vector7.y == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.x == 2.0f && vector3.y == 2.0f, "Vector2D");

			Vector2D vector8 = vector1 * 2.0f;

			UNIT_TEST_POSITIVE(vector8.x == 2.0f && vector8.y == 4.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			
			Vector2D vector9 = vector1 / 2.0f;

			UNIT_TEST_POSITIVE(vector9.x == 0.5f && vector9.y == 1.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 2.0f);
			Vector2D vector2 = vector1.AsInverse();
			
			UNIT_TEST_POSITIVE(vector2.x == -1.0f && vector2.y == -2.0f, "Vector2D");

			vector1.Invert();
			
			UNIT_TEST_POSITIVE(vector1.x == -1.0f && vector1.y == -2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsValid(), "Vector2D");
			
			vector1.Clear();
			
			UNIT_TEST_POSITIVE(vector1.x == 0.0f && vector1.y == 0.0f, "Vector2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 2.0f);
			Vector2D vector2 = vector1.Absolute();
			vector1.Absolutize();

			UNIT_TEST_POSITIVE(vector1.x == 1.0f && vector1.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.x == 1.0f && vector2.y == 2.0f, "Vector2D");

			Vector2D vector3(-1.0f, -2.0f);
			Vector2D vector4 = vector3.Absolute();
			vector3.Absolutize();

			UNIT_TEST_POSITIVE(vector3.x == 1.0f && vector3.y == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.x == 1.0f && vector4.y == 2.0f, "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Vector2D vector1(1.0f, 2.0f);
			Vector2D vector2(2.0f, 3.0f);
			
			float dot = vector1.Dot(vector2);
			float sqMag = vector1.SquareMagnitude();
			float mag = vector1.Magnitude();
			float distTo = vector1.DistanceTo(vector2);
			float sqDistTo = vector1.SquareDistanceTo(vector2);
			float manhattenDistance = vector1.ManhattanDistanceTo(vector2);

			UNIT_TEST_POSITIVE(dot == 8.0f, "Vector2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 1.41421356f), "Vector2D");
			UNIT_TEST_POSITIVE(sqDistTo == 2.0f, "Vector2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 2.0f, "Vector2D");

			Vector2D vector3(-1.0f, -2.0f);
			Vector2D vector4(2.0f, 3.0f);
			
			dot = vector3.Dot(vector4);
			sqMag = vector3.SquareMagnitude();
			mag = vector3.Magnitude();
			distTo = vector3.DistanceTo(vector4);
			sqDistTo = vector3.SquareDistanceTo(vector4);
			manhattenDistance = vector3.ManhattanDistanceTo(vector4);

			UNIT_TEST_POSITIVE(dot == -8.0f, "Vector2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 5.83095189f), "Vector2D");
			UNIT_TEST_POSITIVE(sqDistTo == 34.0f, "Vector2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 8.0f, "Vector2D");

			Vector2D vector5(1.0f, 2.0f);
			Vector2D vector6(-2.0f, -3.0f);

			dot = vector5.Dot(vector6);
			sqMag = vector5.SquareMagnitude();
			mag = vector5.Magnitude();
			distTo = vector5.DistanceTo(vector6);
			sqDistTo = vector5.SquareDistanceTo(vector6);
			manhattenDistance = vector1.ManhattanDistanceTo(vector6);

			UNIT_TEST_POSITIVE(dot == -8.0f, "Vector2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 5.83095189f), "Vector2D");
			UNIT_TEST_POSITIVE(sqDistTo == 34.0f, "Vector2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 8.0f, "Vector2D");

			Vector2D vector7(1.0f, 2.0f);
			Vector2D vector8(1.0f, 2.0f);
			
			dot = vector7.Dot(vector8);
			sqMag = vector7.SquareMagnitude();
			mag = vector7.Magnitude();
			distTo = vector7.DistanceTo(vector8);
			sqDistTo = vector7.SquareDistanceTo(vector8);
			manhattenDistance = vector7.ManhattanDistanceTo(vector8);
					
			UNIT_TEST_POSITIVE(dot == 5.0f, "Vector2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(sqDistTo == 0.0f, "Vector2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 0.0f, "Vector2D");

			Vector2D vector9(-1.0f, -2.0f);
			Vector2D vector10(-1.0f, -2.0f);
			
			dot = vector9.Dot(vector10);
			sqMag = vector9.SquareMagnitude();
			mag = vector9.Magnitude();
			distTo = vector9.DistanceTo(vector10);
			sqDistTo = vector9.SquareDistanceTo(vector10);
			manhattenDistance = vector9.ManhattanDistanceTo(vector10);

			UNIT_TEST_POSITIVE(dot == 5.0f, "Vector2D");
			UNIT_TEST_POSITIVE(sqMag == 5.0f, "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(mag, 2.23606798f), "Vector2D");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(distTo, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(sqDistTo == 0.0f, "Vector2D");
			UNIT_TEST_POSITIVE(manhattenDistance == 0.0f, "Vector2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(0.707106f, 0.707106f);
			Vector2D vector3(0.0f, -1.0f);
			Vector2D vector4(-0.707106f, -0.707106f);
			
			UNIT_TEST_POSITIVE(vector1.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsNormal(), "Vector2D");
			
			Vector2D vector5(1.0f, 0.1f);
			Vector2D vector6(0.5f, 0.5f);
			Vector2D vector7(-0.1f, -1.0f);
			Vector2D vector8(-0.5555f, -0.55555f);
			
			UNIT_TEST_POSITIVE(!vector5.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(!vector6.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(!vector7.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(!vector8.IsNormal(), "Vector2D");
			
			Vector2D vector9 = vector5.AsNormal();
			Vector2D vector10 = vector6.AsNormal();
			Vector2D vector11 = vector7.AsNormal();
			Vector2D vector12 = vector8.AsNormal();
			
			UNIT_TEST_POSITIVE(vector9.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector10.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector11.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector12.IsNormal(), "Vector2D");
			
			vector5.Normalize();
			vector6.Normalize();
			vector7.Normalize();
			vector8.Normalize();
				
			UNIT_TEST_POSITIVE(vector5.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsNormal(), "Vector2D");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector13(0.0f, 0.0f);
			Vector2D vector14 = vector13.AsNormal();		
			UNIT_TEST_ASSERT_EXPECTED_END();	

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector15(0.0f, 0.0f);
			vector15.Normalize();		
			UNIT_TEST_ASSERT_EXPECTED_END();
				
			Vector2D vector16(1.0f, 0.01f);
			Vector2D vector17(0.5111f, 0.5f);
			Vector2D vector18(-0.1f, -1.0f);
			Vector2D vector19(-0.5555f, -0.55555f);
			
			UNIT_TEST_POSITIVE(!vector16.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(!vector17.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(!vector18.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(!vector19.IsNormal(), "Vector2D");
			
			Vector2D vector20 = vector16.AsNormal();
			Vector2D vector21 = vector17.AsNormal();
			Vector2D vector22 = vector18.AsNormal();
			Vector2D vector23 = vector19.AsNormal();
			
			UNIT_TEST_POSITIVE(vector20.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector21.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector22.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector23.IsNormal(), "Vector2D");
			
			vector16.Normalize();
			vector17.Normalize();
			vector18.Normalize();
			vector19.Normalize();
				
			UNIT_TEST_POSITIVE(vector16.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector17.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector18.IsNormal(), "Vector2D");
			UNIT_TEST_POSITIVE(vector19.IsNormal(), "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 2.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(2.0f, 1.0f);
			Vector2D vector4(0.0f, 0.0f);

			UNIT_TEST_POSITIVE(vector1.AsSwitchedAxis() == Vector2D(2.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.AsSwitchedAxis() == Vector2D(1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector3.AsSwitchedAxis() == Vector2D(1.0f, 2.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector4.AsSwitchedAxis() == Vector2D(0.0f, 0.0f), "Vector2D");

			UNIT_TEST_POSITIVE(vector1.SwitchAxis() == Vector2D(2.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.SwitchAxis() == Vector2D(1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector3.SwitchAxis() == Vector2D(1.0f, 2.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector4.SwitchAxis() == Vector2D(0.0f, 0.0f), "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vectorOnTo1(1.0f, 0.0f);
			Vector2D vectorOnTo2(0.0f, 1.0f);
			Vector2D vectorOnTo3(1.0f, 1.0f);
			Vector2D vectorOnTo4(-1.0f, 1.0f);

			{
				Vector2D vector1(0.5f, 0.0f);
				Vector2D vector2(0.5f, 0.5f);
				Vector2D vector3(0.0f, 0.5f);
				Vector2D vector4(-0.5f, 0.5f);
				Vector2D vector5(-0.5f, 0.0f);
				Vector2D vector6(-0.5f, -0.5f);
				Vector2D vector7(0.0f, -0.5f);
				Vector2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo1) == Vector2D(0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo1) == Vector2D(0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo1) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo1) == Vector2D(-0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo1) == Vector2D(-0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo1) == Vector2D(-0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo1) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo1) == Vector2D(0.5f, 0.0f), "Vector2D");
				
				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, -0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, -0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo2) == Vector2D(0.0f, -0.5f), "Vector2D");

				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo3) == Vector2D(0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo3) == Vector2D(0.5f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo3) == Vector2D(0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo3) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo3) == Vector2D(-0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo3) == Vector2D(-0.5f, -0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo3) == Vector2D(-0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo3) == Vector2D(0.0f, 0.0f), "Vector2D");

				UNIT_TEST_POSITIVE(vector1.AsProjectOnTo(vectorOnTo4) == Vector2D(0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.AsProjectOnTo(vectorOnTo4) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.AsProjectOnTo(vectorOnTo4) == Vector2D(-0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.AsProjectOnTo(vectorOnTo4) == Vector2D(-0.5f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.AsProjectOnTo(vectorOnTo4) == Vector2D(-0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.AsProjectOnTo(vectorOnTo4) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.AsProjectOnTo(vectorOnTo4) == Vector2D(0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.AsProjectOnTo(vectorOnTo4) == Vector2D(0.5f, -0.5f), "Vector2D");
			}
			{
				Vector2D vector1(0.5f, 0.0f);
				Vector2D vector2(0.5f, 0.5f);
				Vector2D vector3(0.0f, 0.5f);
				Vector2D vector4(-0.5f, 0.5f);
				Vector2D vector5(-0.5f, 0.0f);
				Vector2D vector6(-0.5f, -0.5f);
				Vector2D vector7(0.0f, -0.5f);
				Vector2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo1) == Vector2D(0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo1) == Vector2D(0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo1) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo1) == Vector2D(-0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo1) == Vector2D(-0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo1) == Vector2D(-0.5f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo1) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo1) == Vector2D(0.5f, 0.0f), "Vector2D");
				
			}
			{
				Vector2D vector1(0.5f, 0.0f);
				Vector2D vector2(0.5f, 0.5f);
				Vector2D vector3(0.0f, 0.5f);
				Vector2D vector4(-0.5f, 0.5f);
				Vector2D vector5(-0.5f, 0.0f);
				Vector2D vector6(-0.5f, -0.5f);
				Vector2D vector7(0.0f, -0.5f);
				Vector2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, -0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, -0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo2) == Vector2D(0.0f, -0.5f), "Vector2D");
			}

			{
				Vector2D vector1(0.5f, 0.0f);
				Vector2D vector2(0.5f, 0.5f);
				Vector2D vector3(0.0f, 0.5f);
				Vector2D vector4(-0.5f, 0.5f);
				Vector2D vector5(-0.5f, 0.0f);
				Vector2D vector6(-0.5f, -0.5f);
				Vector2D vector7(0.0f, -0.5f);
				Vector2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo3) == Vector2D(0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo3) == Vector2D(0.5f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo3) == Vector2D(0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo3) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo3) == Vector2D(-0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo3) == Vector2D(-0.5f, -0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo3) == Vector2D(-0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo3) == Vector2D(0.0f, 0.0f), "Vector2D");
			}

			{
				Vector2D vector1(0.5f, 0.0f);
				Vector2D vector2(0.5f, 0.5f);
				Vector2D vector3(0.0f, 0.5f);
				Vector2D vector4(-0.5f, 0.5f);
				Vector2D vector5(-0.5f, 0.0f);
				Vector2D vector6(-0.5f, -0.5f);
				Vector2D vector7(0.0f, -0.5f);
				Vector2D vector8(0.5f, -0.5f);

				UNIT_TEST_POSITIVE(vector1.ProjectOnTo(vectorOnTo4) == Vector2D(0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector2.ProjectOnTo(vectorOnTo4) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector3.ProjectOnTo(vectorOnTo4) == Vector2D(-0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector4.ProjectOnTo(vectorOnTo4) == Vector2D(-0.5f, 0.5f), "Vector2D");
				UNIT_TEST_POSITIVE(vector5.ProjectOnTo(vectorOnTo4) == Vector2D(-0.25f, 0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector6.ProjectOnTo(vectorOnTo4) == Vector2D(0.0f, 0.0f), "Vector2D");
				UNIT_TEST_POSITIVE(vector7.ProjectOnTo(vectorOnTo4) == Vector2D(0.25f, -0.25f), "Vector2D");
				UNIT_TEST_POSITIVE(vector8.ProjectOnTo(vectorOnTo4) == Vector2D(0.5f, -0.5f), "Vector2D");
			}

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.AsRotated90DegreeCounterClockwise() == Vector2D(0.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.AsRotated90DegreeCounterClockwise() == Vector2D(-1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector3.AsRotated90DegreeCounterClockwise() == Vector2D(-1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector4.AsRotated90DegreeCounterClockwise() == Vector2D(-1.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector5.AsRotated90DegreeCounterClockwise() == Vector2D(0.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector6.AsRotated90DegreeCounterClockwise() == Vector2D(1.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector7.AsRotated90DegreeCounterClockwise() == Vector2D(1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector8.AsRotated90DegreeCounterClockwise() == Vector2D(1.0f, 1.0f), "Vector2D");

			UNIT_TEST_POSITIVE(vector1.Rotate90DegreeCounterClockwise() == Vector2D(0.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.Rotate90DegreeCounterClockwise() == Vector2D(-1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector3.Rotate90DegreeCounterClockwise() == Vector2D(-1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector4.Rotate90DegreeCounterClockwise() == Vector2D(-1.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector5.Rotate90DegreeCounterClockwise() == Vector2D(0.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector6.Rotate90DegreeCounterClockwise() == Vector2D(1.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector7.Rotate90DegreeCounterClockwise() == Vector2D(1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector8.Rotate90DegreeCounterClockwise() == Vector2D(1.0f, 1.0f), "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.AsRotated90DegreeClockwise() == Vector2D(0.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.AsRotated90DegreeClockwise() == Vector2D(1.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector3.AsRotated90DegreeClockwise() == Vector2D(1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector4.AsRotated90DegreeClockwise() == Vector2D(1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector5.AsRotated90DegreeClockwise() == Vector2D(0.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector6.AsRotated90DegreeClockwise() == Vector2D(-1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector7.AsRotated90DegreeClockwise() == Vector2D(-1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector8.AsRotated90DegreeClockwise() == Vector2D(-1.0f, -1.0f), "Vector2D");

			UNIT_TEST_POSITIVE(vector1.Rotate90DegreeClockwise() == Vector2D(0.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector2.Rotate90DegreeClockwise() == Vector2D(1.0f, -1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector3.Rotate90DegreeClockwise() == Vector2D(1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector4.Rotate90DegreeClockwise() == Vector2D(1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector5.Rotate90DegreeClockwise() == Vector2D(0.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector6.Rotate90DegreeClockwise() == Vector2D(-1.0f, 1.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector7.Rotate90DegreeClockwise() == Vector2D(-1.0f, 0.0f), "Vector2D");
			UNIT_TEST_POSITIVE(vector8.Rotate90DegreeClockwise() == Vector2D(-1.0f, -1.0f), "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsClockwiseTo(vector8) == true, "Vector2D");

			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsClockwiseTo(vector8) == true, "Vector2D");

			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsClockwiseTo(vector8) == true, "Vector2D");

			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsClockwiseTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsClockwiseTo(vector8) == false, "Vector2D");
			
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsClockwiseTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsClockwiseTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsClockwiseTo(vector8) == false, "Vector2D");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsCounterClockwiseTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsCounterClockwiseTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsCounterClockwiseTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsCounterClockwiseTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsCounterClockwiseTo(vector8) == true, "Vector2D");
			
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsCounterClockwiseTo(vector8) == true, "Vector2D");

			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsCounterClockwiseTo(vector8) == true, "Vector2D");

			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsCounterClockwiseTo(vector8) == false, "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector1.IsParallelTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector2.IsParallelTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector3.IsParallelTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector4.IsParallelTo(vector8) == true, "Vector2D");

			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector1) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector5) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector5.IsParallelTo(vector8) == false, "Vector2D");
			
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector2) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector6) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector6.IsParallelTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector3) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector4) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector7) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector7.IsParallelTo(vector8) == false, "Vector2D");

			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector1) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector2) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector3) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector4) == true, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector5) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector6) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector7) == false, "Vector2D");
			UNIT_TEST_POSITIVE(vector8.IsParallelTo(vector8) == true, "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);
			
			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Angle result;

			vector1.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");		
			vector1.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector1.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector1.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector1.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector1.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector1.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector1.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");

			vector2.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");		
			vector2.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector2.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector2.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector2.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector2.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector2.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector2.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");

			vector3.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");		
			vector3.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector3.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector3.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector3.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector3.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector3.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector3.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");

			vector4.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");		
			vector4.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector4.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector4.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector4.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector4.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector4.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector4.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
		
			vector5.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");		
			vector5.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector5.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector5.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector5.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector5.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector5.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector5.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");

			vector6.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");		
			vector6.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector6.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector6.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector6.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector6.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector6.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector6.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");

			vector7.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");		
			vector7.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector7.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector7.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector7.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector7.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector7.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector7.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");

			vector8.GetAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");		
			vector8.GetAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector8.GetAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector8.GetAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector8.GetAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector8.GetAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector8.GetAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector8.GetAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(0.0f, 0.0f);
			vector1.GetAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 1.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 1.0f);
			Vector2D vector2(1.0f, 1.0f);
			vector1.GetAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);
			
			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Angle result;

			vector1.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");		
			vector1.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector1.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector1.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector1.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector1.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector1.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector1.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");

			vector2.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");		
			vector2.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector2.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector2.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector2.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector2.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector2.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector2.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");

			vector3.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");		
			vector3.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector3.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector3.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector3.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector3.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector3.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector3.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");

			vector4.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");		
			vector4.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector4.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector4.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector4.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector4.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector4.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector4.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
		
			vector5.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");		
			vector5.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector5.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector5.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector5.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector5.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector5.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector5.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");

			vector6.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");		
			vector6.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector6.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector6.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector6.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector6.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector6.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector6.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");

			vector7.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");		
			vector7.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector7.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector7.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector7.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector7.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector7.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector7.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");

			vector8.GetClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");		
			vector8.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector8.GetClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector8.GetClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector8.GetClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector8.GetClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector8.GetClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector8.GetClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(0.0f, 0.0f);
			vector1.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 1.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 1.0f);
			Vector2D vector2(1.0f, 1.0f);
			vector1.GetClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);
			
			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Angle result;

			vector1.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");		
			vector1.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector1.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector1.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector1.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector1.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector1.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector1.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");

			vector2.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");		
			vector2.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector2.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector2.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector2.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector2.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector2.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector2.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");

			vector3.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");		
			vector3.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector3.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector3.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector3.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector3.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector3.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector3.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");

			vector4.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");		
			vector4.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector4.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector4.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector4.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector4.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector4.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector4.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
		
			vector5.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");		
			vector5.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector5.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector5.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector5.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector5.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector5.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector5.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");

			vector6.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");		
			vector6.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector6.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector6.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector6.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector6.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector6.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");
			vector6.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");

			vector7.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");		
			vector7.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector7.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector7.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector7.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector7.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector7.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");
			vector7.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");

			vector8.GetCounterClockwiseAngleBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg45, "Vector2D");		
			vector8.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg90, "Vector2D");
			vector8.GetCounterClockwiseAngleBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg135, "Vector2D");
			vector8.GetCounterClockwiseAngleBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg180, "Vector2D");
			vector8.GetCounterClockwiseAngleBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg135, "Vector2D");
			vector8.GetCounterClockwiseAngleBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg90, "Vector2D");
			vector8.GetCounterClockwiseAngleBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg360 - Angle::Deg45, "Vector2D");
			vector8.GetCounterClockwiseAngleBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Angle::Deg0, "Vector2D");

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(0.0f, 0.0f);
			vector1.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 1.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 1.0f);
			Vector2D vector2(1.0f, 1.0f);
			vector1.GetCounterClockwiseAngleBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Vector2D resultVec = vector1.AsRotateCounterClockwiseBy(testAngle);

				Angle resultAngle;
				
				vector1.GetCounterClockwiseAngleBetween(resultVec, resultAngle);
				
				Dia::Core::Containers::String128 str("ResultVec %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", resultVec.x, resultVec.y, resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Angle resultAngle;
			vector1.AsRotateCounterClockwiseBy(resultAngle);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vectorBase(1.0f, 0.0f);
			Vector2D vector1(vectorBase);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Vector2D vector1 = vectorBase;

				vector1.RotateCounterClockwiseBy(testAngle);

				Angle resultAngle;
				
				vectorBase.GetCounterClockwiseAngleBetween(vector1, resultAngle);
				
				Dia::Core::Containers::String128 str("ResultVec %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", vector1.x, vector1.y, resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Angle resultAngle;
			vector1.RotateCounterClockwiseBy(resultAngle);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Vector2D resultVec = vector1.AsRotateClockwiseBy(testAngle);

				Angle resultAngle;
				
				vector1.GetClockwiseAngleBetween(resultVec, resultAngle);
				
				Dia::Core::Containers::String128 str("ResultVec %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", resultVec.x, resultVec.y, resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Angle resultAngle;
			vector1.AsRotateClockwiseBy(resultAngle);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vectorBase(1.0f, 0.0f);
			Vector2D vector1(vectorBase);
			
			for (int i = 0; i <= 360; i++)
			{
				Angle testAngle = Dia::Maths::Angle::FromDegrees(static_cast<float>(i));
				
				Vector2D vector1 = vectorBase;

				vector1.RotateClockwiseBy(testAngle);

				Angle resultAngle;
				
				vectorBase.GetClockwiseAngleBetween(vector1, resultAngle);
				
				Dia::Core::Containers::String128 str("ResultVec %0.4f, %0.4f - resultAngle %0.5f, testAngle %0.5f", vector1.x, vector1.y, resultAngle.AsDegrees(), testAngle.AsDegrees());
				UNIT_TEST_POSITIVE(resultAngle == testAngle, str.AsCStr());
			}
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Angle resultAngle;
			vector1.RotateClockwiseBy(resultAngle);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Matrix22 result;

			vector1.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");		
			vector1.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");	
			vector1.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");	
			vector1.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");	
			vector1.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");	
			vector1.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");
			vector1.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");
			vector1.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");

			vector2.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");		
			vector2.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");	
			vector2.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");	
			vector2.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");	
			vector2.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");	
			vector2.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");
			vector2.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");
			vector2.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");

			vector3.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");
			vector3.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");
			vector3.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");
			vector3.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");
			vector3.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");
			vector3.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");
			vector3.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");
			vector3.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");

			vector4.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");		
			vector4.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");
			vector4.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");
			vector4.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");
			vector4.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");
			vector4.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");
			vector4.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");
			vector4.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");

			vector5.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");		
			vector5.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");
			vector5.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");
			vector5.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");
			vector5.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");
			vector5.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");
			vector5.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");
			vector5.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");

			vector6.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");		
			vector6.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");
			vector6.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");
			vector6.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");
			vector6.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");
			vector6.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");
			vector6.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");
			vector6.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");

			vector7.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");	
			vector7.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");
			vector7.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");
			vector7.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");
			vector7.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");
			vector7.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");
			vector7.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");
			vector7.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");

			vector8.GetRotationBetween(vector1, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector2, vector4), "Vector2D");	
			vector8.GetRotationBetween(vector2, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector3, vector5), "Vector2D");
			vector8.GetRotationBetween(vector3, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector4, vector6), "Vector2D");
			vector8.GetRotationBetween(vector4, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector5, vector7), "Vector2D");
			vector8.GetRotationBetween(vector5, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector6, vector8), "Vector2D");
			vector8.GetRotationBetween(vector6, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector7, vector1), "Vector2D");
			vector8.GetRotationBetween(vector7, result);
			UNIT_TEST_POSITIVE(result == Matrix22(vector8, vector2), "Vector2D");
			vector8.GetRotationBetween(vector8, result);
			UNIT_TEST_POSITIVE(result == Matrix22::Identity, "Vector2D");

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 0.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetRotationBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(0.0f, 0.0f);
			vector1.GetRotationBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(1.0f, 1.0f);
			Vector2D vector2(1.0f, 0.0f);
			vector1.GetRotationBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Vector2D vector1(0.0f, 1.0f);
			Vector2D vector2(1.0f, 1.0f);
			vector1.GetRotationBetween(vector2, result);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);


			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);
		
			result = vector1.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector1.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector1.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");
			result = vector1.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = vector1.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = vector1.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector1.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector1.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	

			result = vector2.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector2.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = vector2.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");
			result = vector2.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = vector2.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");		
			result = vector2.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector2.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector2.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	

			result = vector3.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = vector3.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = vector3.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector3.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");
			result = vector3.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");		
			result = vector3.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");		
			result = vector3.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector3.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	

			result = vector4.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = vector4.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector4.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector4.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");
			result = vector4.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");		
			result = vector4.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");		
			result = vector4.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector4.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			
			result = vector5.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector5.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector5.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector5.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");
			result = vector5.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");		
			result = vector5.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");		
			result = vector5.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = vector5.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");

			result = vector6.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector6.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector6.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector6.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");
			result = vector6.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");		
			result = vector6.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");		
			result = vector6.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = vector6.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");

			result = vector7.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector7.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector7.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector7.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");
			result = vector7.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");		
			result = vector7.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = vector7.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector7.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");

			result = vector8.AsMultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector8.AsMultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector8.AsMultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector8.AsMultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");
			result = vector8.AsMultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = vector8.AsMultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = vector8.AsMultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector8.AsMultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Vector2D vector1(1.0f, 0.0f);
			Vector2D vector2(1.0f, 1.0f);
			Vector2D vector3(0.0f, 1.0f);
			Vector2D vector4(-1.0f, 1.0f);
			Vector2D vector5(-1.0f, 0.0f);
			Vector2D vector6(-1.0f, -1.0f);
			Vector2D vector7(0.0f, -1.0f);
			Vector2D vector8(1.0f, -1.0f);

			vector2.Normalize();
			vector4.Normalize();
			vector6.Normalize();
			vector8.Normalize();

			Vector2D result;

			Matrix22 matrix1(vector1, vector3);
			Matrix22 matrix2(vector2, vector4);
			Matrix22 matrix3(vector3, vector5);
			Matrix22 matrix4(vector4, vector6);
			Matrix22 matrix5(vector5, vector7);
			Matrix22 matrix6(vector6, vector8);
			Matrix22 matrix7(vector7, vector1);
			Matrix22 matrix8(vector8, vector2);

			result = vector1;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector1;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector1;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");
			result = vector1;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = vector1;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = vector1;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector1;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector1;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	

			result = vector2;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector2;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = vector2;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");
			result = vector2;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = vector2;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");		
			result = vector2;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector2;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector2;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	

			result = vector3;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = vector3;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = vector3;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector3;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");
			result = vector3;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");		
			result = vector3;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");		
			result = vector3;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector3;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	

			result = vector4;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = vector4;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector4;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector4;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");
			result = vector4;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");		
			result = vector4;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");		
			result = vector4;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector4;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	

			result = vector5;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector5;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector5;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector5;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");
			result = vector5;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");		
			result = vector5;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");		
			result = vector5;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");	
			result = vector5;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");

			result = vector6;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector6;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector6;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector6;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");
			result = vector6;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");		
			result = vector6;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");		
			result = vector6;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");	
			result = vector6;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");

			result = vector7;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");	
			result = vector7;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector7;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector7;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");
			result = vector7;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");		
			result = vector7;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = vector7;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");	
			result = vector7;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");

			result = vector8;
			result.MultipliedBy(matrix1);
			UNIT_TEST_POSITIVE(result == vector8, "Vector2D");	
			result = vector8;
			result.MultipliedBy(matrix2);
			UNIT_TEST_POSITIVE(result == vector1, "Vector2D");	
			result = vector8;
			result.MultipliedBy(matrix3);
			UNIT_TEST_POSITIVE(result == vector2, "Vector2D");	
			result = vector8;
			result.MultipliedBy(matrix4);
			UNIT_TEST_POSITIVE(result == vector3, "Vector2D");
			result = vector8;
			result.MultipliedBy(matrix5);
			UNIT_TEST_POSITIVE(result == vector4, "Vector2D");		
			result = vector8;
			result.MultipliedBy(matrix6);
			UNIT_TEST_POSITIVE(result == vector5, "Vector2D");		
			result = vector8;
			result.MultipliedBy(matrix7);
			UNIT_TEST_POSITIVE(result == vector6, "Vector2D");	
			result = vector8;
			result.MultipliedBy(matrix8);
			UNIT_TEST_POSITIVE(result == vector7, "Vector2D");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
