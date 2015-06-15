#include "DiaCore/Timer/Timer.h"

#include "DiaCore/Core/Assert.h"
namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS Timer
		//==============================================================================
		Timer::Timer()
			: mStartTime(Dia::Core::TimeAbsolute::MinimumTime())
			, mTimerLength(Dia::Core::TimeRelative::Zero())
		{}

		void Timer::Reset()
		{
			mStartTime = TimeAbsolute::MinimumTime();
			mTimerLength = TimeRelative::Zero();	
		}

		void Timer::Start( const TimeAbsolute& timeNow )
		{
			mStartTime = timeNow;
			mTimerLength = TimeRelative::Zero();
		}
		
		void Timer::Stop( const TimeAbsolute& timeNow )
		{
			DIA_ASSERT(IsRunning(), "Must start a timer before stopping it");

			mTimerLength = timeNow - mStartTime;
			mStartTime = TimeAbsolute::MinimumTime();
		}

		bool Timer::IsRunning()const
		{
			return (mStartTime > TimeAbsolute::MinimumTime());
		}

		TimeRelative Timer::IsRunningFor(const TimeAbsolute& timeNow)const
		{
			if (IsRunning())
			{
				return timeNow - mStartTime;
			}
			else
			{
				return mTimerLength;
			}
		}

		const TimeAbsolute& Timer::StartTime()const
		{
			return mStartTime;
		}
	}
}