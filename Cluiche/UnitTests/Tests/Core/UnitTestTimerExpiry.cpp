
#include "UnitTests/Tests/Core/UnitTestTimerExpiry.h"

#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include <DiaCore/Timer/TimerExpiry.h>
#include <DiaCore/Time/TimeServer.h>

namespace UnitTests
{	
	/* 	class TimerExpiry
		{
		public:

			TimeRelative			TimeSinceExpiry( const TimeAbsolute& timeNow ) const;            
			TimeRelative			TimeUntilExpiry( const TimeAbsolute& timeNow ) const;
		            
		protected:
			TimeAbsolute			mExpiryTime;
			bool                    mIsSet;
			*/
	UnitTestTimerExpiry::UnitTestTimerExpiry(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestTimerExpiry::UnitTestTimerExpiry(void)
		: UnitTestCore()
	{}

	void UnitTestTimerExpiry::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimerExpiry timer;
	
			UNIT_TEST_POSITIVE(timer.IsRunning(Dia::Core::TimeAbsolute::Zero()) == false, "TimerExpiry");
			
		UNIT_TEST_BLOCK_END()
		
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimerExpiry timer;
			
			timer.Start(Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::CreateFromSeconds(60.0f));

			UNIT_TEST_POSITIVE(timer.IsRunning(Dia::Core::TimeAbsolute::Zero()) == true, "TimerExpiry");
			
			timer.Reset();
			
			UNIT_TEST_POSITIVE(timer.IsRunning(Dia::Core::TimeAbsolute::Zero()) == false, "TimerExpiry");
		
		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimerExpiry timer;
			
			timer.Start(Dia::Core::TimeAbsolute::Zero() + Dia::Core::TimeRelative::CreateFromSeconds(0.1f));

			UNIT_TEST_POSITIVE(timer.IsRunning(Dia::Core::TimeAbsolute::Zero()) == true, "TimerExpiry");
			
			timer.Reset();
			
			UNIT_TEST_POSITIVE(timer.IsRunning(Dia::Core::TimeAbsolute::Zero()) == false, "TimerExpiry");

		UNIT_TEST_BLOCK_END()
	
		UNIT_TEST_BLOCK_START()
			
			Dia::Core::TimerExpiry timer;
			Dia::Core::TimeServer time(60.0f, Dia::Core::TimeAbsolute::Zero());

			Dia::Core::TimeAbsolute expireAt = time.GetTime() + Dia::Core::TimeRelative::CreateFromSeconds(0.1f);
			timer.Start(expireAt);
			
			time.Tick();
		
			UNIT_TEST_POSITIVE(timer.IsRunning(time.GetTime()) == true, "TimerExpiry");
			UNIT_TEST_POSITIVE(timer.TimeUntilExpiry(time.GetTime()) == (expireAt - time.GetTime()), "TimerExpiry");
			
			UNIT_TEST_ASSERT_EXPECTED_START();
			timer.TimeSinceExpiry(time.GetTime());
			UNIT_TEST_ASSERT_EXPECTED_END();

			for (int i = 0; i < 1000; i++)
			{
				time.Tick();
			}
		
			UNIT_TEST_POSITIVE(timer.IsRunning(time.GetTime()) == false, "TimerExpiry");

		UNIT_TEST_BLOCK_END()

		mState = kFinished;
	}
}
