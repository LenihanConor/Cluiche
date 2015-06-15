#pragma once

#include "DiaCore/Time/TimeAbsolute.h"
#include "DiaCore/Time/TimeRelative.h"

namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS TimeServer
		//==============================================================================
		class TimeServer
		{
		public:
			TimeServer();
			TimeServer( float hz, const TimeAbsolute &timeNow );		

			TimeServer(const TimeServer& rhs);
			TimeServer& operator=(const TimeServer& rhs);

			void					Create(float hz, const TimeAbsolute &timeNow);

			void					Reset();

			void					Tick();

			const TimeAbsolute&		GetTime() const;
			const TimeAbsolute&		GetLastTime() const;
			const TimeRelative&		GetStep() const;

			TimeRelative			GetNextStep() const;
			TimeRelative			GetLastStep() const;			

			int						GetTick() const;

			float					GetTimeScale() const;

			void					SetTimeScale( float scale );
			void					AdjustTimeScale( float adjScale );
		
		private:
			TimeAbsolute			mTime;						// What is the current time for the server, this increments in steps
			TimeRelative			mTimeStep;					// Length of the time step 
			TimeAbsolute			mLastTime;					// Previous time before the step			
			TimeAbsolute			mSystemTimeOfNextTick;		// Next systme time we will step forward 

			float					mTimeScale;					// Allows us to speed up and slow down time, only applied on the next tick
			float					mQueuedTimeScale;			// Time scale to be applied next scale

			unsigned int			mTick;						// What is the current "frame" number we are on
		};
	}
}
