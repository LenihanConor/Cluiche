#include "DiaCore/Timer/TimerExpiry.h"

#include "DiaCore/Core/Assert.h"

namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS Timer
		//==============================================================================
		TimerExpiry::TimerExpiry()
			:   mExpiryTime( TimeAbsolute::MinimumTime() )
		{}

		void TimerExpiry::Start( const TimeAbsolute& timeNow, const TimeRelative& duration )
		{
			Start( timeNow + duration );
		}
		
		void TimerExpiry::Start( const TimeAbsolute& expiryTime )
		{
			DIA_ASSERT( mExpiryTime == TimeAbsolute::MinimumTime(), "Set a timer that is already set" );
			
			mExpiryTime = expiryTime;
		}

		void TimerExpiry::Reset()
		{
			mExpiryTime = TimeAbsolute::MinimumTime();
		}

		bool TimerExpiry::IsRunning(const TimeAbsolute& timeNow) const
		{
			return ((mExpiryTime != TimeAbsolute::MinimumTime()) && (mExpiryTime > timeNow));
		}

		TimeRelative TimerExpiry::TimeSinceExpiry( const TimeAbsolute& timeNow ) const
		{
			DIA_ASSERT( !IsRunning(timeNow), "Timer has not expired" );
		    
			return timeNow - mExpiryTime;
		}

		TimeRelative TimerExpiry::TimeUntilExpiry( const TimeAbsolute& timeNow ) const
		{
			DIA_ASSERT( IsRunning(timeNow), "Time has expired" );
		    
			return mExpiryTime - timeNow;
		}
	}
}