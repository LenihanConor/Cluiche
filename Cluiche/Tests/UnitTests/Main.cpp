#include <stdio.h>

#include "UnitTests/Infrastructure/UnitTestManager.h"
#include <DiaCore/Time/SystemClock.h>

int main( int argc, const char* argv[] )
{
	Dia::Core::SystemClock clock;
	
	UnitTests::UnitTestManager::Create();
	UnitTests::UnitTestManager& unitTestManager = UnitTests::UnitTestManager::GetInstance();

	unitTestManager.Init();
	unitTestManager.Test(clock);	
	unitTestManager.Flush();
	
	UnitTests::UnitTestManager::Destroy();
}