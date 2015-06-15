
#include "UnitTests/Tests/Core/UnitTestTimeServer.h"

#include <DiaCore/Time/TimeServer.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestTimeServer::UnitTestTimeServer(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestTimeServer::UnitTestTimeServer(void)
		: UnitTestCore()
	{}

	void UnitTestTimeServer::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimeServer timeServer(60.0f, Dia::Core::TimeAbsolute::Zero());
				
			UNIT_TEST_POSITIVE(timeServer.GetTime() == Dia::Core::TimeAbsolute::Zero(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer.GetTime() == timeServer.GetLastTime(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer.GetStep() == Dia::Core::TimeRelative::CreateFromSeconds(1.0f / 60.0f), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer.GetLastStep() == Dia::Core::TimeRelative::Zero(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer.GetNextStep() == timeServer.GetStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer.GetTick() == 0, "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer.GetTimeScale() == 1.0f, "TimeServer()");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimeServer timeServer;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			timeServer.Tick();
			UNIT_TEST_ASSERT_EXPECTED_END();
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimeServer timeServer1;
			Dia::Core::TimeServer timeServer2(60.0f, Dia::Core::TimeAbsolute::Zero());
			
			timeServer1.Create(60.0f, Dia::Core::TimeAbsolute::Zero());

			UNIT_TEST_POSITIVE(timeServer1.GetTime() == timeServer2.GetTime(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetLastTime() == timeServer2.GetLastTime(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetStep() == timeServer2.GetStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetLastStep() == timeServer2.GetLastStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetNextStep() == timeServer2.GetNextStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetTick() == timeServer2.GetTick(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetTimeScale() == timeServer2.GetTimeScale(), "TimeServer()");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimeServer timeServer1;
			Dia::Core::TimeServer timeServer2(60.0f, Dia::Core::TimeAbsolute::Zero());
			
			timeServer1 = timeServer2;

			UNIT_TEST_POSITIVE(timeServer1.GetTime() == timeServer2.GetTime(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetLastTime() == timeServer2.GetLastTime(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetStep() == timeServer2.GetStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetLastStep() == timeServer2.GetLastStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetNextStep() == timeServer2.GetNextStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetTick() == timeServer2.GetTick(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetTimeScale() == timeServer2.GetTimeScale(), "TimeServer()");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
					
			Dia::Core::TimeServer timeServer2(60.0f, Dia::Core::TimeAbsolute::Zero());
			Dia::Core::TimeServer timeServer1(timeServer2);

			UNIT_TEST_POSITIVE(timeServer1.GetTime() == timeServer2.GetTime(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetLastTime() == timeServer2.GetLastTime(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetStep() == timeServer2.GetStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetLastStep() == timeServer2.GetLastStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetNextStep() == timeServer2.GetNextStep(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetTick() == timeServer2.GetTick(), "TimeServer()");
			UNIT_TEST_POSITIVE(timeServer1.GetTimeScale() == timeServer2.GetTimeScale(), "TimeServer()");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
					
			Dia::Core::TimeServer timeServer(60.0f, Dia::Core::TimeAbsolute::Zero());
			
			timeServer.Tick();

			UNIT_TEST_POSITIVE(timeServer.GetTime() == Dia::Core::TimeAbsolute::Zero() + timeServer.GetStep(), "TimeServer.Tick");
			UNIT_TEST_POSITIVE(timeServer.GetLastTime() == Dia::Core::TimeAbsolute::Zero(), "TimeServer.Tick");
			UNIT_TEST_POSITIVE(timeServer.GetTick() == 1, "TimeServer.Tick");
			
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
					
			Dia::Core::TimeServer timeServer(60.0f, Dia::Core::TimeAbsolute::Zero());
			
			timeServer.Tick();
			timeServer.Reset();

			UNIT_TEST_POSITIVE(timeServer.GetTime() == Dia::Core::TimeAbsolute::Zero(), "TimeServer.Reset");
			UNIT_TEST_POSITIVE(timeServer.GetLastTime() == Dia::Core::TimeAbsolute::Zero(), "TimeServer.Reset");
			UNIT_TEST_POSITIVE(timeServer.GetTick() == 0, "TimeServer()");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
				
			Dia::Core::TimeServer timeServer(60.0f, Dia::Core::TimeAbsolute::Zero());
			
			timeServer.SetTimeScale(0.5f);

			timeServer.Tick();
			timeServer.Tick();
		
			UNIT_TEST_POSITIVE(timeServer.GetTime() == Dia::Core::TimeAbsolute::Zero() + timeServer.GetStep() + ( timeServer.GetStep() * 0.5f ), "TimeServer.Tick");
			UNIT_TEST_POSITIVE(timeServer.GetLastTime() == Dia::Core::TimeAbsolute::Zero() + timeServer.GetStep(), "TimeServer.Tick");
			UNIT_TEST_POSITIVE(timeServer.GetTick() == 2, "TimeServer.Tick");
				
		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()
				
			Dia::Core::TimeServer timeServer(60.0f, Dia::Core::TimeAbsolute::Zero());
			
			timeServer.AdjustTimeScale(0.5f);

			timeServer.Tick();
			timeServer.Tick();
		
			UNIT_TEST_POSITIVE(timeServer.GetTime() == Dia::Core::TimeAbsolute::Zero() + timeServer.GetStep() + ( timeServer.GetStep() * 1.5f ), "TimeServer.Tick");
			UNIT_TEST_POSITIVE(timeServer.GetLastTime() == Dia::Core::TimeAbsolute::Zero() + timeServer.GetStep(), "TimeServer.Tick");
			UNIT_TEST_POSITIVE(timeServer.GetTick() == 2, "TimeServer.Tick");
				
		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
