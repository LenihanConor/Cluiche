#include "UnitTests/Tests/Core/UnitTestMetaLogic.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaCore/Core/MetaLogic.h>

using namespace Dia::Core;

namespace UnitTests
{	
	UnitTestMetaLogic::UnitTestMetaLogic(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestMetaLogic::UnitTestMetaLogic(void)
		: UnitTestCore()
	{}

	void UnitTestMetaLogic::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			bool or1 = Dia::Core::MetaOr<true, true>::value;
			bool or2 = Dia::Core::MetaOr<true, false>::value;
			bool or3 = Dia::Core::MetaOr<false, false>::value;
			UNIT_TEST_POSITIVE( or1, "Metalogic");
			UNIT_TEST_POSITIVE( or2, "Metalogic");
 			UNIT_TEST_NEGATIVE( or3, "Metalogic");

			bool and1 = Dia::Core::MetaAnd<true, true>::value;
			bool and2 = Dia::Core::MetaAnd<true, false>::value;
			bool and3 = Dia::Core::MetaAnd<false, false>::value;
			UNIT_TEST_POSITIVE( and1, "Metalogic");
			UNIT_TEST_NEGATIVE( and2, "Metalogic");
			UNIT_TEST_NEGATIVE( and3, "Metalogic");
			
			bool not1 = Dia::Core::MetaNot<true>::value;
			bool not2 = Dia::Core::MetaNot<false>::value;
			UNIT_TEST_NEGATIVE( not1, "Metalogic");
			UNIT_TEST_POSITIVE( not2, "Metalogic");
			
 			int max1 = Dia::Core::MetaMax<int, 3,4>::value;
			int min1 = Dia::Core::MetaMin<int, 3,4>::value;
			UNIT_TEST_POSITIVE( max1 == 4, "Metalogic");
			UNIT_TEST_POSITIVE( min1 == 3, "Metalogic");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}