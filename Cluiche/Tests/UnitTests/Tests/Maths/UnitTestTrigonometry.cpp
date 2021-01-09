
#include "UnitTests/Tests/Maths/UnitTestTrigonometry.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaMaths/Core/Trigonometry.h>

namespace UnitTests
{	
	UnitTestTrigonometry::UnitTestTrigonometry(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestTrigonometry::UnitTestTrigonometry(void)
		: UnitTestMaths()
	{}

	void UnitTestTrigonometry::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(0.0f) == 0.0f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(45.0f) == 0.785398163f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(90.0f) == 1.57079633f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(135.0f) == 2.35619449f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(180.0f) == 3.14159265f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::DegToRadians(-45.0f) == -0.785398163f, "Trigonometry");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			UNIT_TEST_POSITIVE(Dia::Maths::RadiansToDeg(0.0f) == 0.0f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::RadiansToDeg(0.785398163f) == 45.0f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::RadiansToDeg(1.57079633f) == 90.0f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::RadiansToDeg(2.35619449f) == 135.0f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::RadiansToDeg(3.14159265f) == 180.0f, "Trigonometry");
			UNIT_TEST_POSITIVE(Dia::Maths::RadiansToDeg(-0.785398163f) == -45.0f, "Trigonometry");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			float angle = -2.0f * Dia::Maths::PI;
			while (angle <= (2.0f * Dia::Maths::PI)) 
			{
				Dia::Maths::Angle angleClass(angle);
				Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);

				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Sin(angle), sinf(angle)), str.AsCStr());
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Sin(angleClass.AsRadians()), sinf(angle)), str.AsCStr());
				angle += 0.03f;
			}

			angle = (2.0f * Dia::Maths::PI);
			Dia::Maths::Angle angleClass(angle);
			Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Sin(angle), sinf(angle)), str.AsCStr());
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Sin(angleClass.AsRadians()), sinf(angle)), str.AsCStr());
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			float angle = -2.0f * Dia::Maths::PI;
			while (angle <= (2.0f * Dia::Maths::PI)) 
			{
				Dia::Maths::Angle angleClass(angle);
				Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Cos(angle), cosf(angle)), str.AsCStr());
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Cos(angleClass.AsRadians()), cosf(angle)), str.AsCStr());
				angle += 0.03f;
			}

			angle = (2.0f * Dia::Maths::PI);
			Dia::Maths::Angle angleClass(angle);
			Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Cos(angle), cosf(angle)), str.AsCStr());
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Cos(angleClass.AsRadians()), cosf(angle)), str.AsCStr());
	
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float angle = -2.0f * Dia::Maths::PI;
			while (angle <= (2.0f * Dia::Maths::PI)) 
			{
				Dia::Maths::Angle angleClass(angle);
				Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
			
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Tan(angle), tanf(angle), 0.005f), str.AsCStr());
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Tan(angleClass.AsRadians()), tanf(angle), 0.005f), str.AsCStr());
				angle += 0.03f;
			}

			angle = (2.0f * Dia::Maths::PI);
			Dia::Maths::Angle angleClass(angle);
			Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Tan(angle), tanf(angle)), str.AsCStr());
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::Tan(angleClass.AsRadians()), tanf(angle)), str.AsCStr());
	
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float angle = -1.0f;
			while (angle < 1.0f) 
			{
				Dia::Maths::Angle angleClass(angle);
				Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ACos(angle), acosf(angle)), str.AsCStr());
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ACos(angleClass.AsRadians()), acosf(angle)), str.AsCStr());
				angle += 0.03f;
			}

			angle = 1.0f;
			Dia::Maths::Angle angleClass(angle);
			Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ACos(angle), acosf(angle)), str.AsCStr());
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ACos(angleClass.AsRadians()), acosf(angle)), str.AsCStr());

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float angle = -1.0f;
			while (angle <= (1.0f)) 
			{
				Dia::Maths::Angle angleClass(angle);
				Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ASin(angle), asinf(angle)), str.AsCStr());
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ASin(angleClass.AsRadians()), asinf(angle)), str.AsCStr());
				angle += 0.03f;
			}
			angle = 1.0f;
			Dia::Maths::Angle angleClass(angle);
			Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ASin(angle), asinf(angle)), str.AsCStr());
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ASin(angleClass.AsRadians()), asinf(angle)), str.AsCStr());
	
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float angle = -Dia::Maths::PI_HALF;
			while (angle <= (Dia::Maths::PI_HALF)) 
			{
				Dia::Maths::Angle angleClass(angle);
				Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ATan(angle), atanf(angle)), str.AsCStr());
				UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ATan(angleClass.AsRadians()), atanf(angle)), str.AsCStr());
				angle += 0.03f;
			}
			angle = (Dia::Maths::PI_HALF);
			Dia::Maths::Angle angleClass(angle);
			Dia::Core::Containers::String32 str("Trigonometry: %.4f", angle);
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ATan(angle), atanf(angle)), str.AsCStr());
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(Dia::Maths::ATan(angleClass.AsRadians()), atanf(angle)), str.AsCStr());
	
		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
