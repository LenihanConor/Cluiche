
#include "UnitTests/Tests/Core/UnitTestTimer.h"

#include <DiaCore/Timer/Timer.h>
#include <DiaCore/Time/TimeServer.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestTimer::UnitTestTimer(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestTimer::UnitTestTimer(void)
		: UnitTestCore()
	{}

	void UnitTestTimer::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::Timer timer;
	
			UNIT_TEST_POSITIVE(timer.IsRunning() == false, "Timer");
			UNIT_TEST_POSITIVE(timer.IsRunningFor(Dia::Core::TimeAbsolute::Zero()) == Dia::Core::TimeRelative::Zero(), "Timer");
	
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Core::Timer timer;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			timer.Stop(Dia::Core::TimeAbsolute::Zero());
			UNIT_TEST_ASSERT_EXPECTED_END();	

			timer.Start(Dia::Core::TimeAbsolute::Zero());
			timer.Stop(Dia::Core::TimeAbsolute::Zero());

			UNIT_TEST_POSITIVE(timer.IsRunning() == false, "Timer");
			UNIT_TEST_POSITIVE(timer.IsRunningFor(Dia::Core::TimeAbsolute::Zero()) == Dia::Core::TimeRelative::Zero(), "Timer");
	
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::Timer timer1;
			Dia::Core::Timer timer2;

			timer2.Reset();

			UNIT_TEST_POSITIVE(timer1.StartTime() == timer2.StartTime(), "Timer");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Core::Timer timer;
			Dia::Core::TimeServer time(60.0f, Dia::Core::TimeAbsolute::Zero());

			timer.Start(time.GetTime());
			
			time.Tick();
		
			UNIT_TEST_POSITIVE(timer.IsRunning() == true, "Timer");
			UNIT_TEST_POSITIVE(timer.IsRunningFor(time.GetTime()) == time.GetStep(), "Timer");

			time.Tick();

			timer.Stop(time.GetTime());
			
			UNIT_TEST_POSITIVE(timer.IsRunning() == false, "Timer");
			UNIT_TEST_POSITIVE(timer.IsRunningFor(time.GetTime()) == (time.GetStep() * 2.0f), "Timer");

			timer.Reset();
			
			UNIT_TEST_POSITIVE(timer.IsRunning() == false, "Timer");
			UNIT_TEST_POSITIVE(timer.IsRunningFor(time.GetTime()) == Dia::Core::TimeRelative::Zero(), "Timer");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
