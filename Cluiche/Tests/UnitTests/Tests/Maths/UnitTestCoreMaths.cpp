
#include "UnitTests/Tests/Maths/UnitTestCoreMaths.h"

#include <DiaMaths/Core/CoreMaths.h>
#include <DiaMaths/Core/FloatMaths.h>
#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestCoreMaths::UnitTestCoreMaths(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestCoreMaths::UnitTestCoreMaths(void)
		: UnitTestMaths()
	{}

	void UnitTestCoreMaths::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Clamp(3, 1, 5);
			int result2 = Dia::Maths::Clamp(0, 1, 5);
			int result3 = Dia::Maths::Clamp(6, 1, 5);
			
			UNIT_TEST_POSITIVE(result1 == 3, "Clamp");
			UNIT_TEST_POSITIVE(result2 == 1, "Clamp");
			UNIT_TEST_POSITIVE(result3 == 5, "Clamp");
			
		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Abs(0);
			int result2 = Dia::Maths::Abs(-11);
			int result3 = Dia::Maths::Abs(11);
			
			UNIT_TEST_POSITIVE(result1 == 0, "Abs");
			UNIT_TEST_POSITIVE(result2 == 11, "Abs");
			UNIT_TEST_POSITIVE(result3 == 11, "Abs");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Negative(0);
			int result2 = Dia::Maths::Negative(-11);
			int result3 = Dia::Maths::Negative(11);
			
			UNIT_TEST_POSITIVE(result1 == 0, "Negative");
			UNIT_TEST_POSITIVE(result2 == -11, "Negative");
			UNIT_TEST_POSITIVE(result3 == -11, "Negative");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Min(1, 2);
			int result2 = Dia::Maths::Min(2, 1);
			int result3 = Dia::Maths::Min(1, 1);
			
			UNIT_TEST_POSITIVE(result1 == 1, "Min");
			UNIT_TEST_POSITIVE(result2 == 1, "Min");
			UNIT_TEST_POSITIVE(result3 == 1, "Min");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Max(1, 2);
			int result2 = Dia::Maths::Max(2, 1);
			int result3 = Dia::Maths::Max(1, 1);
			
			UNIT_TEST_POSITIVE(result1 == 2, "Max");
			UNIT_TEST_POSITIVE(result2 == 2, "Max");
			UNIT_TEST_POSITIVE(result3 == 1, "Max");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Square(2);
			float result2 = Dia::Maths::Square(2.5f);
			int result3 = Dia::Maths::Square(0);
			int result4 = Dia::Maths::Square(-1);

			UNIT_TEST_POSITIVE(result1 == 4, "Square");
			UNIT_TEST_POSITIVE(result2 == 6.25f, "Square");
			UNIT_TEST_POSITIVE(result3 == 0, "Square");
			UNIT_TEST_POSITIVE(result4 == 1, "Square");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Power(2, 3);
			float result2 = Dia::Maths::Power(2.5f, 3);
			int result3 = Dia::Maths::Power(2, 1);
			int result4 = Dia::Maths::Power(2, 0);
			int result5 = Dia::Maths::Power(0, 2);
			int result6 = Dia::Maths::Power(-1, 2);
			int result7 = Dia::Maths::Power(-1, 3);

			UNIT_TEST_POSITIVE(result1 == 8, "Power");
			UNIT_TEST_POSITIVE(result2 == 15.625f, "Power");
			UNIT_TEST_POSITIVE(result3 == 2, "Power");
			UNIT_TEST_POSITIVE(result4 == 1, "Power");
			UNIT_TEST_POSITIVE(result5 == 0, "Power");
			UNIT_TEST_POSITIVE(result6 == 1, "Power");
			UNIT_TEST_POSITIVE(result7 == -1, "Power");

		UNIT_TEST_BLOCK_END()
					
		UNIT_TEST_BLOCK_START()
			
			int a = 2;
			int b = 3;

			Dia::Maths::Swap(a, b);

			UNIT_TEST_POSITIVE(a == 3, "Swap");
			UNIT_TEST_POSITIVE(b == 2, "Swap");
	
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::SquareRoot(9.0f);
			float result2 = Dia::Maths::SquareRoot(5.0f);
			float result3 = Dia::Maths::SquareRoot(0.0f);

			UNIT_TEST_POSITIVE(result1 == 3, "SquareRoot");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result2, 2.2360679f), "SquareRoot");
			UNIT_TEST_POSITIVE(result3 == 0, "SquareRoot");
	
			UNIT_TEST_ASSERT_EXPECTED_START();		
			Dia::Maths::SquareRoot(-1.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();	

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			int result1 = Dia::Maths::Factorial(4);
			int result2 = Dia::Maths::Factorial(1);
			int result3 = Dia::Maths::Factorial(0);

			UNIT_TEST_POSITIVE(result1 == 24, "Factorial");
			UNIT_TEST_POSITIVE(result2 == 1, "Factorial");
			UNIT_TEST_POSITIVE(result3 == 1, "Factorial");
	
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Factorial(-2);
			UNIT_TEST_ASSERT_EXPECTED_END();	

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
