#include "DiaCore/Timer/TimerSystem.h"

#include "DiaCore/Time/SystemClock.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS TimerSystem
		//==============================================================================	
		TimerSystem::TimerSystem()
			: mStartTime(Dia::Core::TimeAbsolute::MinimumTime())
			, mTimerLength(Dia::Core::TimeRelative::Zero())
		{}

		void TimerSystem::Reset()
		{
			mStartTime = TimeAbsolute::MinimumTime();
			mTimerLength = TimeRelative::Zero();	
		}

		void TimerSystem::Start()
		{
			mStartTime = CurrentTime();
			mTimerLength = TimeRelative::Zero();
		}

		void TimerSystem::Stop()
		{
			DIA_ASSERT(IsRunning(), "Must start a timer before stopping it");

			mTimerLength = CurrentTime() - mStartTime;
			mStartTime = TimeAbsolute::MinimumTime();
		}

		bool TimerSystem::IsRunning()const
		{
			return (mStartTime > TimeAbsolute::Zero());
		}

		TimeRelative TimerSystem::IsRunningFor()const
		{
			if (IsRunning())
			{
				return CurrentTime() - mStartTime;
			}
			else
			{
				return mTimerLength;
			}
		}

		TimeAbsolute TimerSystem::CurrentTime()const
		{
			return sSystemClock.CurrentTime();
		}

		const TimeAbsolute& TimerSystem::StartTime()const
		{
			return mStartTime;
		}
	}
}