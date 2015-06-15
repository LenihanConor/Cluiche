#pragma once

#include "DiaCore/Time/TimeAbsolute.h"
#include "DiaCore/Time/TimeRelative.h"

namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS TimerExpiry
		//==============================================================================
		class TimerExpiry
		{
		public:

			TimerExpiry();

			void					Start( const TimeAbsolute& timeNow, const TimeRelative& duration );
			void					Start( const TimeAbsolute&  expiryTime );
			
			void					Reset();

			bool					IsRunning(const TimeAbsolute& timeNow) const;

			TimeRelative			TimeSinceExpiry( const TimeAbsolute& timeNow ) const;            
			TimeRelative			TimeUntilExpiry( const TimeAbsolute& timeNow ) const;
		            
		protected:
			TimeAbsolute			mExpiryTime;
		};
	}
}