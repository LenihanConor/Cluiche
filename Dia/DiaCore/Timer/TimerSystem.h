#pragma once

#include "DiaCore/Time/TimeAbsolute.h"
#include "DiaCore/Time/TimeRelative.h"

namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS TimerSystem
		//==============================================================================
		class TimerSystem
		{
		public:
			TimerSystem();

			void			Reset();

			void			Start();
		    void			Stop();
			
			bool			IsRunning()const;
			TimeRelative	IsRunningFor()const;

			const TimeAbsolute& StartTime()const;
		private:
			TimeAbsolute	CurrentTime()const;

			TimeAbsolute     mStartTime;
			TimeRelative     mTimerLength;
		};
	}
}