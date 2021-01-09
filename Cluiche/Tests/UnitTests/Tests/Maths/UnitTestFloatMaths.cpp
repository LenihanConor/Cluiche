
#include "UnitTests/Tests/Maths/UnitTestFloatMaths.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaMaths/Core/FloatMaths.h>

namespace UnitTests
{	
	UnitTestFloatMaths::UnitTestFloatMaths(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestFloatMaths::UnitTestFloatMaths(void)
		: UnitTestMaths()
	{}

	void UnitTestFloatMaths::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FPower(2.0f, 3.0f);
			float result2 = Dia::Maths::Float::FPower(2.5f, 3.0f);
			float result3 = Dia::Maths::Float::FPower(2.0f, 1.0f);
			float result4 = Dia::Maths::Float::FPower(2.0f, 0.0f);
			float result5 = Dia::Maths::Float::FPower(0.0f, 2.0f);
			float result6 = Dia::Maths::Float::FPower(-1.0f, 2.0f);
			float result7 = Dia::Maths::Float::FPower(-1.0f, 3.0f);

			UNIT_TEST_POSITIVE(result1 == 8.0f, "FPower");
			UNIT_TEST_POSITIVE(result2 == 15.625f, "FPower");
			UNIT_TEST_POSITIVE(result3 == 2.0f, "FPower");
			UNIT_TEST_POSITIVE(result4 == 1.0f, "FPower");
			UNIT_TEST_POSITIVE(result5 == 0.0f, "FPower");
			UNIT_TEST_POSITIVE(result6 == 1.0f, "FPower");
			UNIT_TEST_POSITIVE(result7 == -1.0f, "FPower");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FPower(2.0f, 3);
			float result2 = Dia::Maths::Float::FPower(2.5f, 3);
			float result3 = Dia::Maths::Float::FPower(2.0f, 1);
			float result4 = Dia::Maths::Float::FPower(2.0f, 0);
			float result5 = Dia::Maths::Float::FPower(0.0f, 2);
			float result6 = Dia::Maths::Float::FPower(-1.0f, 2);
			float result7 = Dia::Maths::Float::FPower(-1.0f, 3);

			UNIT_TEST_POSITIVE(result1 == 8.0f, "FPower");
			UNIT_TEST_POSITIVE(result2 == 15.625f, "FPower");
			UNIT_TEST_POSITIVE(result3 == 2.0f, "FPower");
			UNIT_TEST_POSITIVE(result4 == 1.0f, "FPower");
			UNIT_TEST_POSITIVE(result5 == 0.0f, "FPower");
			UNIT_TEST_POSITIVE(result6 == 1.0f, "FPower");
			UNIT_TEST_POSITIVE(result7 == -1.0f, "FPower");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FSquare(2.0f);
			float result2 = Dia::Maths::Float::FSquare(2.5f);
			float result3 = Dia::Maths::Float::FSquare(0.0f);
			float result4 = Dia::Maths::Float::FSquare(-1.0f);

			UNIT_TEST_POSITIVE(result1 == 4.0f, "FSquare");
			UNIT_TEST_POSITIVE(result2 == 6.25f, "FSquare");
			UNIT_TEST_POSITIVE(result3 == 0.0f, "FSquare");
			UNIT_TEST_POSITIVE(result4 == 1.0f, "FSquare");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FSquareRoot(9.0f);
			float result2 = Dia::Maths::Float::FSquareRoot(5.0f);
			float result3 = Dia::Maths::Float::FSquareRoot(0.0f);

			UNIT_TEST_POSITIVE(result1 == 3, "FSquareRoot");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result2, 2.2360679f), "FSquareRoot");
			UNIT_TEST_POSITIVE(result3 == 0, "FSquareRoot");
	
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::SquareRoot(-1.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();	

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FLog(2.0f);

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result1, 0.6931471805f), "FLog");
		
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLog(0.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLog(-1.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FLog10(2.0f);

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result1, 0.3010299f), "FLog");
		
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLog10(0.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLog10(-1.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FLogBase(3.0f, 2.0f);

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result1, 1.5849625f), "FLogBase");
		
			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLogBase(0.0f, 2.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();	

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLogBase(-1.0f, 2.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLogBase(2.0f, 0.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FLogBase(2.0f, -1.0f);
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FExp(-1.0f);
			float result2 = Dia::Maths::Float::FExp(0);
			float result3 = Dia::Maths::Float::FExp(1.0f);
			float result4 = Dia::Maths::Float::FExp(-1.0f);

			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result1, 0.367879441f), "FExp");
			UNIT_TEST_POSITIVE(result2 == 1.0f, "FExp");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result3, 2.71828183f), "FExp");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(result4, 0.36787945f), "FExp");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FAbs(0);
			float result2 = Dia::Maths::Float::FAbs(-11);
			float result3 = Dia::Maths::Float::FAbs(11);
			
			UNIT_TEST_POSITIVE(result1 == 0, "FAbs");
			UNIT_TEST_POSITIVE(result2 == 11, "FAbs");
			UNIT_TEST_POSITIVE(result3 == 11, "FAbs");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FNegative(0);
			float result2 = Dia::Maths::Float::FNegative(-11);
			float result3 = Dia::Maths::Float::FNegative(11);
			
			UNIT_TEST_POSITIVE(result1 == 0, "FNegative");
			UNIT_TEST_POSITIVE(result2 == -11, "FNegative");
			UNIT_TEST_POSITIVE(result3 == -11, "FNegative");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FFloor(0.0f);
			float result2 = Dia::Maths::Float::FFloor(1.1f);
			float result3 = Dia::Maths::Float::FFloor(1.9f);
			float result4 = Dia::Maths::Float::FFloor(-1.1f);
			float result5 = Dia::Maths::Float::FFloor(-1.9f);

			UNIT_TEST_POSITIVE(result1 == 0.0f, "FFloor");
			UNIT_TEST_POSITIVE(result2 == 1.0f, "FFloor");
			UNIT_TEST_POSITIVE(result3 == 1.0f, "FFloor");
			UNIT_TEST_POSITIVE(result4 == -2.0f, "FFloor");
			UNIT_TEST_POSITIVE(result5 == -2.0f, "FFloor");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FCeiling(0.0f);
			float result2 = Dia::Maths::Float::FCeiling(1.1f);
			float result3 = Dia::Maths::Float::FCeiling(1.9f);
			float result4 = Dia::Maths::Float::FCeiling(-1.1f);
			float result5 = Dia::Maths::Float::FCeiling(-1.9f);

			UNIT_TEST_POSITIVE(result1 == 0.0f, "FCeiling");
			UNIT_TEST_POSITIVE(result2 == 2.0f, "FCeiling");
			UNIT_TEST_POSITIVE(result3 == 2.0f, "FCeiling");
			UNIT_TEST_POSITIVE(result4 == -1.0f, "FCeiling");
			UNIT_TEST_POSITIVE(result5 == -1.0f, "FCeiling");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FRound(0.0f);
			float result2 = Dia::Maths::Float::FRound(1.1f);
			float result3 = Dia::Maths::Float::FRound(1.9f);
			float result4 = Dia::Maths::Float::FRound(-1.1f);
			float result5 = Dia::Maths::Float::FRound(-1.9f);

			UNIT_TEST_POSITIVE(result1 == 0.0f, "FRound");
			UNIT_TEST_POSITIVE(result2 == 1.0f, "FRound");
			UNIT_TEST_POSITIVE(result3 == 2.0f, "FRound");
			UNIT_TEST_POSITIVE(result4 == -1.0f, "FRound");
			UNIT_TEST_POSITIVE(result5 == -2.0f, "FRound");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			float result1 = Dia::Maths::Float::FTruncate(1.23456789f, 1);
			float result2 = Dia::Maths::Float::FTruncate(1.23456789f, 3);
			float result3 = Dia::Maths::Float::FTruncate(-1.23456789f, 1);
			float result4 = Dia::Maths::Float::FTruncate(1.23456789f, 0);			

			UNIT_TEST_POSITIVE(result1 == 1.2f, "FTruncate");
			UNIT_TEST_POSITIVE(result2 == 1.234f, "FTruncate");
			UNIT_TEST_POSITIVE(result3 == -1.2f, "FTruncate");
			UNIT_TEST_POSITIVE(result4 == 1.0f, "FTruncate");

			UNIT_TEST_ASSERT_EXPECTED_START();
			Dia::Maths::Float::FTruncate(1.23456789f, -1);
			UNIT_TEST_ASSERT_EXPECTED_END();	
			
		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(1.0f, 1.0f), "FEqual");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FEqual(1.0f, 1.2f), "FEqual");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqual(-1.0f, -1.0f), "FEqual");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FEqual(-1.0f, -1.2f), "FEqual");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLess(1.0f, 1.2f), "FLess");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLess(1.2f, 1.0f), "FLess");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLess(-1.2f, -1.0f), "FLess");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLess(-1.0f, -1.2f), "FLess");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreater(1.2f, 1.0f), "FGreater");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreater(1.0f, 1.2f), "FGreater");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreater(-1.0f, -1.2f), "FGreater");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreater(-1.2f, -1.0f), "FGreater");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqual(1.0f, 1.2f), "FLessEqual");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqual(1.0f, 1.0f), "FLessEqual");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLessEqual(1.2f, 1.0f), "FLessEqual");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqual(-1.2f, -1.0f), "FLessEqual");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqual(-1.0f, -1.0f), "FLessEqual");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLessEqual(-1.0f, -1.2f), "FLessEqual");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqual(1.2f, 1.0f), "FGreaterEqual");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqual(1.0f, 1.0f), "FGreaterEqual");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreaterEqual(1.0f, 1.2f), "FGreaterEqual");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqual(-1.0f, -1.2f), "FGreaterEqual");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqual(-1.0f, -1.0f), "FGreaterEqual");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreaterEqual(-1.2f, -1.0f), "FGreaterEqual");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqualRelative(1.0f, 1.0f), "FEqualRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FEqualRelative(1.0f, 1.2f), "FEqualRelative");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FEqualRelative(-1.0f, -1.0f), "FEqualRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FEqualRelative(-1.0f, -1.2f), "FEqualRelative");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessRelative(1.0f, 1.2f), "FLessRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLessRelative(1.2f, 1.0f), "FLessRelative");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessRelative(-1.2f, -1.0f), "FLessRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLessRelative(-1.0f, -1.2f), "FLessRelative");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterRelative(1.2f, 1.0f), "FGreaterRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreaterRelative(1.0f, 1.2f), "FGreaterRelative");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterRelative(-1.0f, -1.2f), "FGreaterRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreaterRelative(-1.2f, -1.0f), "FGreaterRelative");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqualRelative(1.0f, 1.2f), "FLessEqualRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqualRelative(1.0f, 1.0f), "FLessEqualRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLessEqualRelative(1.2f, 1.0f), "FLessEqualRelative");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqualRelative(-1.2f, -1.0f), "FLessEqualRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FLessEqualRelative(-1.0f, -1.0f), "FLessEqualRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FLessEqualRelative(-1.0f, -1.2f), "FLessEqualRelative");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqualRelative(1.2f, 1.0f), "FGreaterEqualRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqualRelative(1.0f, 1.0f), "FGreaterEqualRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreaterEqualRelative(1.0f, 1.2f), "FGreaterEqualRelative");
			
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqualRelative(-1.0f, -1.2f), "FGreaterEqualRelative");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FGreaterEqualRelative(-1.0f, -1.0f), "FGreaterEqualRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FGreaterEqualRelative(-1.2f, -1.0f), "FGreaterEqualRelative");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FInRange(1.0f, 1.5f, 1.25f), "FInRange");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FInRange(1.0f, 1.2f, 1.5f), "FInRange");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FInRangeRelative(1.0f, 1.5f, 1.25f), "FInRangeRelative");
			UNIT_TEST_POSITIVE(!Dia::Maths::Float::FInRangeRelative(1.0f, 1.2f, 1.5f), "FInRangeRelative");
			
		UNIT_TEST_BLOCK_END()
			
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FMod(10.0f, 4.0f) == 2.0f, "FMod");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FMod(-10.0f, 4.0f) == -2.0f, "FMod");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FMod(10.0f, -4.0f) == 2.0f, "FMod");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FMod(-10.0f, -4.0f) == -2.0f, "FMod");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
						
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FSelect(10.0f, 1.0f, 2.0f) == 1.0f, "FSelect");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FSelect(-10.0f, 1.0f, 2.0f) == 2.0f, "FSelect");
			UNIT_TEST_POSITIVE(Dia::Maths::Float::FSelect(0.0f, 1.0f, 2.0f) == 1.0f, "FSelect");
			
		UNIT_TEST_BLOCK_END()
	
		mState = kFinished;
	}
}
