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

			void					Pause();
			void					Resume();
			bool					IsPaused() const;

			void					Step(TimeRelative step);
			void					AdvanceTo(TimeAbsolute target);	// Forward-only: ignored if target <= current time

			// Rewind-capable; for restoring saved state only. Gameplay must use AdvanceTo/Step.
			// Sets the clock to t (earlier or later). mLastTime is also set to t so GetLastStep()
			// is zero, and the tick counter is incremented so tick-change observers see an update.
			// Pause and time-scale state are untouched.
			void					SetTime(TimeAbsolute t);

		private:
			TimeAbsolute			mTime;						// What is the current time for the server, this increments in steps
			TimeRelative			mTimeStep;					// Length of the time step
			TimeAbsolute			mLastTime;					// Previous time before the step

			float					mTimeScale;					// Allows us to speed up and slow down time, only applied on the next tick
			float					mQueuedTimeScale;			// Time scale to be applied next scale

			unsigned int			mTick;						// What is the current "frame" number we are on

			bool					mIsPaused;					// When true, Tick() is a no-op
		};
	}
}
