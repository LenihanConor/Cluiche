#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Architecture/Singleton/Singleton.h>

#include "UnitTests/Infrastructure/UnitTestInterface.h"

namespace Dia
{
	namespace Core
	{
		class SystemClock;
	}
}

namespace UnitTests
{
	class UnitTestManager : public Dia::Core::Singleton<UnitTestManager>
	{
	public:
		UnitTestManager(void);
		~UnitTestManager(void);
		
		void Init();
		void Test(const Dia::Core::SystemClock& clock);
		void Flush();

		const UnitTestInterface*	CurrentUnitTest()const;
		UnitTestInterface*			CurrentUnitTest();
	private:
		static const int kMaxTests = 1024;

		UnitTestInterface* mCurrentTest;

		Dia::Core::Containers::DynamicArrayC<UnitTestInterface*, kMaxTests> mUnitTestArray; 
	};
}
