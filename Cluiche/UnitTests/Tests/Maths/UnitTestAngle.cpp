
#include "UnitTests/Tests/Maths/UnitTestAngle.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaMaths/Core/Angle.h>

namespace UnitTests
{	
	UnitTestAngle::UnitTestAngle(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestAngle::UnitTestAngle(void)
		: UnitTestMaths()
	{}

	void UnitTestAngle::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg0 == Dia::Maths::Angle::FromDegrees(0.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::DegHalf == Dia::Maths::Angle::FromDegrees(0.5f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg1 == Dia::Maths::Angle::FromDegrees(1.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg5 == Dia::Maths::Angle::FromDegrees(5.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg10 == Dia::Maths::Angle::FromDegrees(10.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg15 == Dia::Maths::Angle::FromDegrees(15.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg30 == Dia::Maths::Angle::FromDegrees(30.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg45 == Dia::Maths::Angle::FromDegrees(45.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg60 == Dia::Maths::Angle::FromDegrees(60.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg90 == Dia::Maths::Angle::FromDegrees(90.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg120 == Dia::Maths::Angle::FromDegrees(120.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg135 == Dia::Maths::Angle::FromDegrees(135.0f), "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == Dia::Maths::Angle::FromDegrees(180.0f), "Angle");
		
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1;
			Dia::Maths::Angle angle2(Dia::Maths::PI);
			Dia::Maths::Angle angle3(angle2);
			Dia::Maths::Angle angle4 = angle3;

			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg0 == angle1, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle2, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle3, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle4, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::Angle angle1 = -Dia::Maths::Angle::Deg90;
			Dia::Maths::Angle angle2 = -Dia::Maths::Angle::Deg180;	
				
			UNIT_TEST_POSITIVE(angle1.AsDegrees() == -90.0f, "Angle");
			UNIT_TEST_POSITIVE(angle2.AsDegrees() == -180.0f, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg180 - Dia::Maths::Angle::Deg90;
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::Deg0 - Dia::Maths::Angle::Deg90;
			Dia::Maths::Angle angle3 = -Dia::Maths::Angle::Deg180 - Dia::Maths::Angle::Deg90;
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::Deg360 - Dia::Maths::Angle::Deg90;	
				
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg90 == angle1, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle2, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg90 == angle3, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle4, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg90 + Dia::Maths::Angle::Deg90;
			Dia::Maths::Angle angle2 = -Dia::Maths::Angle::Deg180 + Dia::Maths::Angle::Deg90;
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::Deg180 + Dia::Maths::Angle::Deg90;
			Dia::Maths::Angle angle4 = -Dia::Maths::Angle::Deg360 + Dia::Maths::Angle::Deg90;	
				
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle1, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle2, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle3, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg90 == angle4, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg90 * 2.0f;
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::Deg90 * 0.5f;
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::Deg90 * -2.0f;
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::Deg90 * 10.0f;
			Dia::Maths::Angle angle5 = Dia::Maths::Angle::Deg90 * -10.0f;
				
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle1, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg45 == angle2, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg180 == angle3, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle4, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg180 == angle5, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg90 / 2.0f;
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::Deg90 / 0.5f;
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::Deg90 / -2.0f;
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::Deg90 / 10.0f;
			Dia::Maths::Angle angle5 = Dia::Maths::Angle::Deg90 / -10.0f;
			Dia::Maths::Angle angle6 = Dia::Maths::Angle::Deg90 / 0.1f;
			Dia::Maths::Angle angle7 = Dia::Maths::Angle::Deg90 / -0.1f;

			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg45 == angle1, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle2, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg45 == angle3, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::FromDegrees(9.0f) == angle4, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::FromDegrees(9.0f) == angle5, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle6, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg180 == angle7, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg90;
			angle1 += Dia::Maths::Angle::Deg90;

			Dia::Maths::Angle angle2 = -Dia::Maths::Angle::Deg180;
			angle2 += Dia::Maths::Angle::Deg90;

			Dia::Maths::Angle angle3 = Dia::Maths::Angle::Deg180;
			angle3 += Dia::Maths::Angle::Deg90;

			Dia::Maths::Angle angle4 = -Dia::Maths::Angle::Deg360;
			angle4 += Dia::Maths::Angle::Deg90;	
				
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle1, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle2, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle3, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg90 == angle4, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg180;
			angle1 -= Dia::Maths::Angle::Deg90;

			Dia::Maths::Angle angle2 = Dia::Maths::Angle::Deg0;
			angle2 -= Dia::Maths::Angle::Deg90;

			Dia::Maths::Angle angle3 = -Dia::Maths::Angle::Deg180;
			angle3 -= Dia::Maths::Angle::Deg90;

			Dia::Maths::Angle angle4 = Dia::Maths::Angle::Deg360;
			angle4 -= Dia::Maths::Angle::Deg90;	
				
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg90 == angle1, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle2, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg90 == angle3, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg90 == angle4, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg90;
			angle1 *= 2.0f;

			Dia::Maths::Angle angle2 = Dia::Maths::Angle::Deg90;
			angle2 *= 0.5f;

			Dia::Maths::Angle angle3 = Dia::Maths::Angle::Deg90;
			angle3 *= -2.0f;

			Dia::Maths::Angle angle4 = Dia::Maths::Angle::Deg90;
			angle4 *= 10.0f;

			Dia::Maths::Angle angle5 = Dia::Maths::Angle::Deg90;
			angle5 *= -10.0f;
				
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle1, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg45 == angle2, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg180 == angle3, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle4, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg180 == angle5, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg90;
			angle1 /= 2.0f;

			Dia::Maths::Angle angle2 = Dia::Maths::Angle::Deg90;
			angle2 /= 0.5f;

			Dia::Maths::Angle angle3 = Dia::Maths::Angle::Deg90;
			angle3 /= -2.0f;

			Dia::Maths::Angle angle4 = Dia::Maths::Angle::Deg90;
			angle4 /= 10.0f;

			Dia::Maths::Angle angle5 = Dia::Maths::Angle::Deg90;
			angle5 /= -10.0f;

			Dia::Maths::Angle angle6 = Dia::Maths::Angle::Deg90;
			angle6 /= 0.1f;

			Dia::Maths::Angle angle7 = Dia::Maths::Angle::Deg90;
			angle7 /= -0.1f;

			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg45 == angle1, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle2, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg45 == angle3, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::FromDegrees(9.0f) == angle4, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::FromDegrees(9.0f) == angle5, "Angle");
			UNIT_TEST_POSITIVE(Dia::Maths::Angle::Deg180 == angle6, "Angle");
			UNIT_TEST_POSITIVE(-Dia::Maths::Angle::Deg180 == angle7, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::Deg90;
				
			UNIT_TEST_POSITIVE(angle1 == Dia::Maths::Angle::Deg90, "Angle");
			UNIT_TEST_POSITIVE(angle1 != Dia::Maths::Angle::Deg45, "Angle");
			UNIT_TEST_POSITIVE(angle1 <= Dia::Maths::Angle::Deg135, "Angle");
			UNIT_TEST_POSITIVE(angle1 >= Dia::Maths::Angle::Deg45, "Angle");
			UNIT_TEST_POSITIVE(angle1 <= Dia::Maths::Angle::Deg90, "Angle");
			UNIT_TEST_POSITIVE(angle1 >= Dia::Maths::Angle::Deg90, "Angle");
			UNIT_TEST_POSITIVE(angle1 < Dia::Maths::Angle::Deg135, "Angle");
			UNIT_TEST_POSITIVE(angle1 > Dia::Maths::Angle::Deg45, "Angle");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::FromDegrees(90.0f);
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::FromDegrees(-90.0f);
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::FromDegrees(360.0f);
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::FromDegrees(-360.0f);

			UNIT_TEST_POSITIVE(angle1 == Dia::Maths::Angle::Deg90, "Angle");
			UNIT_TEST_POSITIVE(angle2 == -Dia::Maths::Angle::Deg90, "Angle");
			UNIT_TEST_POSITIVE(angle3 == Dia::Maths::Angle::Deg0, "Angle");
			UNIT_TEST_POSITIVE(angle4 == Dia::Maths::Angle::Deg0, "Angle");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::FromRadians(Dia::Maths::PI_HALF);
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::FromRadians(-Dia::Maths::PI_HALF);
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::FromRadians(Dia::Maths::PI_2);
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::FromRadians(-Dia::Maths::PI_2);

			UNIT_TEST_POSITIVE(angle1 == Dia::Maths::Angle::Deg90, "Angle");
			UNIT_TEST_POSITIVE(angle2 == -Dia::Maths::Angle::Deg90, "Angle");
			UNIT_TEST_POSITIVE(angle3 == Dia::Maths::Angle::Deg0, "Angle");
			UNIT_TEST_POSITIVE(angle4 == Dia::Maths::Angle::Deg0, "Angle");
		
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::FromRadians(Dia::Maths::PI_HALF);
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::FromRadians(-Dia::Maths::PI_HALF);
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::FromRadians(Dia::Maths::PI_2);
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::FromRadians(-Dia::Maths::PI_2);

			UNIT_TEST_POSITIVE(angle1.AsRadians() == Dia::Maths::PI_HALF, "Angle");
			UNIT_TEST_POSITIVE(angle2.AsRadians() == -Dia::Maths::PI_HALF, "Angle");
			UNIT_TEST_POSITIVE(angle3.AsRadians() == 0, "Angle");
			UNIT_TEST_POSITIVE(angle4.AsRadians() == 0, "Angle");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::FromDegrees(90.0f);
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::FromDegrees(-90.0f);
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::FromDegrees(360.0f);
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::FromDegrees(-360.0f);

			UNIT_TEST_POSITIVE(angle1.AsDegrees() == 90.0f, "Angle");
			UNIT_TEST_POSITIVE(angle2.AsDegrees() == -90.0f, "Angle");
			UNIT_TEST_POSITIVE(angle3.AsDegrees() == 0, "Angle");
			UNIT_TEST_POSITIVE(angle4.AsDegrees() == 0, "Angle");
		
		UNIT_TEST_BLOCK_END()
		
			UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::FromRadians(Dia::Maths::PI_HALF);
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::FromRadians(-Dia::Maths::PI_HALF);
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::FromRadians(Dia::Maths::PI_2);
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::FromRadians(-Dia::Maths::PI_2);

			UNIT_TEST_POSITIVE(angle1.AsPositiveRadians() == Dia::Maths::PI_HALF, "Angle");
			UNIT_TEST_POSITIVE(angle2.AsPositiveRadians() == Dia::Maths::PI_HALF, "Angle");
			UNIT_TEST_POSITIVE(angle3.AsPositiveRadians() == 0, "Angle");
			UNIT_TEST_POSITIVE(angle4.AsPositiveRadians() == 0, "Angle");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::Angle angle1 = Dia::Maths::Angle::FromDegrees(90.0f);
			Dia::Maths::Angle angle2 = Dia::Maths::Angle::FromDegrees(-90.0f);
			Dia::Maths::Angle angle3 = Dia::Maths::Angle::FromDegrees(360.0f);
			Dia::Maths::Angle angle4 = Dia::Maths::Angle::FromDegrees(-360.0f);

			UNIT_TEST_POSITIVE(angle1.AsPositiveDegrees() == 90.0f, "Angle");
			UNIT_TEST_POSITIVE(angle2.AsPositiveDegrees() == 90.0f, "Angle");
			UNIT_TEST_POSITIVE(angle3.AsPositiveDegrees() == 0, "Angle");
			UNIT_TEST_POSITIVE(angle4.AsPositiveDegrees() == 0, "Angle");
		
		UNIT_TEST_BLOCK_END()
			/*
            inline  void	Normalize();

			
		*/
		mState = kFinished;
	}
}
