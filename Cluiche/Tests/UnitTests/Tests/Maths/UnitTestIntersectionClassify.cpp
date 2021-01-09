
#include "UnitTests/Tests/Maths/UnitTestIntersectionClassify.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaMaths/Shape/Common/IntersectionClassify.h>

using Dia::Maths::IntersectionClassify;

namespace UnitTests
{	
	UnitTestIntersectionClassify::UnitTestIntersectionClassify(const Dia::Core::Containers::String32& name)
		: UnitTestMaths(name)
	{}

	UnitTestIntersectionClassify::UnitTestIntersectionClassify(void)
		: UnitTestMaths()
	{}

	void UnitTestIntersectionClassify::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			IntersectionClassify classify1;
			
			UNIT_TEST_POSITIVE(!classify1.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify1.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify1.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify1.IsNotContainment(), "IntersectionClassify");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()

			IntersectionClassify classify1(IntersectionClassify::kNoIntersection);
			IntersectionClassify classify2(IntersectionClassify::kPenatrating);
			IntersectionClassify classify3(IntersectionClassify::kAContainsB);
			IntersectionClassify classify4(IntersectionClassify::kBContainsA);

			UNIT_TEST_POSITIVE(!classify1.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify1.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify1.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify1.IsNotContainment(), "IntersectionClassify");
			
			UNIT_TEST_POSITIVE(classify2.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify2.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify2.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify2.IsNotContainment(), "IntersectionClassify");

			UNIT_TEST_POSITIVE(classify3.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify3.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify3.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify3.IsNotContainment(), "IntersectionClassify");

			UNIT_TEST_POSITIVE(classify4.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify4.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify4.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify4.IsNotContainment(), "IntersectionClassify");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			IntersectionClassify classify1; classify1.SetClassification(IntersectionClassify::kNoIntersection);
			IntersectionClassify classify2; classify2.SetClassification(IntersectionClassify::kPenatrating);
			IntersectionClassify classify3; classify3.SetClassification(IntersectionClassify::kAContainsB);
			IntersectionClassify classify4; classify4.SetClassification(IntersectionClassify::kBContainsA);

			UNIT_TEST_POSITIVE(classify1.GetClassification() == IntersectionClassify::kNoIntersection, "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify2.GetClassification() == IntersectionClassify::kPenatrating, "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify3.GetClassification() == IntersectionClassify::kAContainsB, "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify4.GetClassification() == IntersectionClassify::kBContainsA, "IntersectionClassify");

			UNIT_TEST_POSITIVE(!classify1.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify1.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify1.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify1.IsNotContainment(), "IntersectionClassify");

			UNIT_TEST_POSITIVE(classify2.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify2.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify2.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify2.IsNotContainment(), "IntersectionClassify");

			UNIT_TEST_POSITIVE(classify3.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify3.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify3.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify3.IsNotContainment(), "IntersectionClassify");

			UNIT_TEST_POSITIVE(classify4.IsIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify4.IsNotIntersecting(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify4.IsContainment(), "IntersectionClassify");
			UNIT_TEST_POSITIVE(!classify4.IsNotContainment(), "IntersectionClassify");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()

			IntersectionClassify classify1(IntersectionClassify::kNoIntersection);
			IntersectionClassify classify2(IntersectionClassify::kPenatrating);
			IntersectionClassify classify3;;

			classify3 = classify2;

			UNIT_TEST_POSITIVE(classify1 != classify2, "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify1 != classify3, "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify3 == classify2, "IntersectionClassify");

		UNIT_TEST_BLOCK_END()
			
		UNIT_TEST_BLOCK_START()

			IntersectionClassify classify1(IntersectionClassify::kAContainsB);
			IntersectionClassify classify2(IntersectionClassify::kBContainsA);

			IntersectionClassify classify3(IntersectionClassify::kAContainsB);
			IntersectionClassify classify4(IntersectionClassify::kBContainsA);

			classify2.ReInterpretAandBObject();
			classify3.ReInterpretAandBObject();

			UNIT_TEST_POSITIVE(classify1 == classify2, "IntersectionClassify");
			UNIT_TEST_POSITIVE(classify3 == classify4, "IntersectionClassify");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
