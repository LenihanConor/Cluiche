
#include "UnitTests/Tests/Core/UnitTestTimerSystem.h"


#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestTimerSystem::UnitTestTimerSystem(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestTimerSystem::UnitTestTimerSystem(void)
		: UnitTestCore()
		, mNumberFrames(0)
	{}

	void UnitTestTimerSystem::DoStart()
	{
		mNumberFrames = 0;

		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimerSystem timer;
	
			UNIT_TEST_POSITIVE(timer.IsRunning() == false, "Timer");
			UNIT_TEST_POSITIVE(timer.IsRunningFor() == Dia::Core::TimeRelative::Zero(), "Timer");
	
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimerSystem timer;
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			timer.Stop();
			UNIT_TEST_ASSERT_EXPECTED_END();	

			timer.Start();
			timer.Stop();

			UNIT_TEST_POSITIVE(timer.IsRunning() == false, "Timer");
			UNIT_TEST_POSITIVE(timer.IsRunningFor() == Dia::Core::TimeRelative::Zero(), "Timer");
	
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimerSystem timer1;
			Dia::Core::TimerSystem timer2;

			timer2.Reset();

			UNIT_TEST_POSITIVE(timer1.StartTime() == timer2.StartTime(), "Timer");

		UNIT_TEST_BLOCK_END()
	}

	void UnitTestTimerSystem::DoTest()
	{
		if (mNumberFrames == 10)
		{
			mState = kFinished;
			return;
		}

		UNIT_TEST_BLOCK_START()
			
			if (mNumberFrames == 0)
			{
				mTestTimer.Start();
				UNIT_TEST_POSITIVE(mTestTimer.IsRunning() == true, "Timer");
			}
			else if (mNumberFrames == 8)
			{
				mTestTimer.Stop();
			
				UNIT_TEST_POSITIVE(mTestTimer.IsRunning() == false, "Timer");
			}

		UNIT_TEST_BLOCK_END()

		mNumberFrames++;
	}
}
