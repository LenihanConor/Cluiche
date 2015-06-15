
#include "UnitTests/Tests/Maths/UnitTestHalfFloat.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaMaths/Core/HalfFloat.h>

namespace UnitTests
{	
	UnitTestHalfFloat::UnitTestHalfFloat(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestHalfFloat::UnitTestHalfFloat(void)
		: UnitTestMaths()
	{}

	void UnitTestHalfFloat::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::HalfFloat a;
			
			UNIT_TEST_POSITIVE(a == 0.0f, "HalfFloat");
			
			Dia::Maths::HalfFloat b(2.0f);
			
			UNIT_TEST_POSITIVE(b == 2.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(b == Dia::Maths::HalfFloat(2.0f), "HalfFloat");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			Dia::Maths::HalfFloat a(1.0f);	
			Dia::Maths::HalfFloat b = -a;

			UNIT_TEST_POSITIVE(a == 1.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(b == -1.0f, "HalfFloat");

			Dia::Maths::HalfFloat c = Dia::Maths::HalfFloat(2.0f);

			UNIT_TEST_POSITIVE(c == 2.0f, "HalfFloat");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::HalfFloat a(1.0f);
			Dia::Maths::HalfFloat b(2.0f);
			Dia::Maths::HalfFloat c = a + b;
			Dia::Maths::HalfFloat d = a - b;

			UNIT_TEST_POSITIVE(a == 1.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(b == 2.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(c == 3.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(d == -1.0f, "HalfFloat");

			Dia::Maths::HalfFloat e; 
			Dia::Maths::HalfFloat f;
			
			e += Dia::Maths::HalfFloat(2.0f);
			f += a;

			UNIT_TEST_POSITIVE(e == 2.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(f == 1.0f, "HalfFloat");

			Dia::Maths::HalfFloat g; 
			Dia::Maths::HalfFloat h;

			g -= Dia::Maths::HalfFloat(2.0f);
			h -= a;

			UNIT_TEST_POSITIVE(g == -2.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(h == -1.0f, "HalfFloat");
			
			Dia::Maths::HalfFloat i(1.0f);

			i = -i;
			
			UNIT_TEST_POSITIVE(i == -1.0f, "HalfFloat");

		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Dia::Maths::HalfFloat a(4.0f);		
			Dia::Maths::HalfFloat b(2.0f);
			Dia::Maths::HalfFloat c = a * b;
			Dia::Maths::HalfFloat d = a / b;

			UNIT_TEST_POSITIVE(a == 4.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(b == 2.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(c == 8.0f, "HalfFloat");
			UNIT_TEST_POSITIVE(d == 2.0f, "HalfFloat");

			Dia::Maths::HalfFloat e(6.0f); 

			e /= Dia::Maths::HalfFloat(2.0f);

			UNIT_TEST_POSITIVE(e == 3.0f, "HalfFloat");

			Dia::Maths::HalfFloat g(3.0f); 
	
			g *= Dia::Maths::HalfFloat(2.0f);
			
			UNIT_TEST_POSITIVE(g == 6.0f, "HalfFloat");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			Dia::Maths::HalfFloat a(1.235f);
			Dia::Maths::HalfFloat b = a.Round(2);
			float c = b;

			UNIT_TEST_POSITIVE(b == 1.25f, "HalfFloat");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
