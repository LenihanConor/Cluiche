#pragma once

#include "DiaCore/Time/TimeAbsolute.h"
#include "DiaCore/Time/TimeRelative.h"

namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS Timer
		//==============================================================================
		class Timer
		{
		public:
			Timer();
			
			void			Reset();

			void			Start( const TimeAbsolute& timeNow );
		    void			Stop( const TimeAbsolute& timeNow );
			
			bool			IsRunning()const;
			TimeRelative	IsRunningFor(const TimeAbsolute& timeNow)const;
			
			const TimeAbsolute& StartTime()const;
		private:
			TimeAbsolute     mStartTime;
			TimeRelative     mTimerLength;
		};
	}
}