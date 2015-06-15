
#include "UnitTests/Tests/Collections/UnitTestSingleton.h"

#include <DiaCore/Architecture/Singleton/Singleton.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestSingleton::UnitTestSingleton(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestSingleton::UnitTestSingleton(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestSingleton::DoTest()
	{
		class MySingleton: public Dia::Core::Singleton<MySingleton>
		{
		public:

		};

		UNIT_TEST_BLOCK_START()
	
			UNIT_TEST_POSITIVE(!MySingleton::IsCreated(), "Singleton");

			UNIT_TEST_ASSERT_EXPECTED_START();
			MySingleton::GetInstanceConst();
			UNIT_TEST_ASSERT_EXPECTED_END();

			MySingleton::Create();

			UNIT_TEST_POSITIVE(MySingleton::IsCreated(), "Singleton")

			UNIT_TEST_ASSERT_EXPECTED_START();
			MySingleton::Create();
			UNIT_TEST_ASSERT_EXPECTED_END();
			
			MySingleton::Destroy();

			UNIT_TEST_POSITIVE(!MySingleton::IsCreated(), "Singleton")

			UNIT_TEST_ASSERT_EXPECTED_START();
			MySingleton::GetInstanceConst();
			UNIT_TEST_ASSERT_EXPECTED_END();

			UNIT_TEST_ASSERT_EXPECTED_START();
			MySingleton::Destroy();
			UNIT_TEST_ASSERT_EXPECTED_END();

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
