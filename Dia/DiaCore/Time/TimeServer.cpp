#include "DiaCore/Time/TimeServer.h"

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Time/SystemClock.h"

#include <chrono>
#include <thread>

namespace Dia
{	
	namespace Core
	{
		//------------------------------------------------------------------------------
		TimeServer::TimeServer()
			: mTime( TimeAbsolute::MinimumTime() )
			, mTimeStep( TimeRelative::MinimumTime() )
			, mLastTime( TimeAbsolute::Zero() )
			, mSystemTimeOfNextTick( TimeAbsolute::Zero() )
			, mTimeScale(1.0f)
			, mQueuedTimeScale( 0.0f )
			, mTick(0)
		{}
	
		//------------------------------------------------------------------------------
		TimeServer::TimeServer( float hz, const TimeAbsolute &timeNow )
			: mTime( timeNow )
			, mTimeStep( TimeRelative::CreateFromSeconds(1.0f / hz) )
			, mLastTime( mTime )
			, mSystemTimeOfNextTick( TimeAbsolute::Zero() )
			, mTimeScale(1.0f)
			, mQueuedTimeScale( mTimeScale )
			, mTick(0)
		{
			DIA_ASSERT(mTimeStep > Dia::Core::TimeRelative::Zero(), "step is too small" );	
		}

		//------------------------------------------------------------------------------
		TimeServer::TimeServer(const TimeServer& rhs)
			: mTime( rhs.mTime )
			, mTimeStep( rhs.mTimeStep )
			, mLastTime( rhs.mLastTime )
			, mSystemTimeOfNextTick(rhs.mSystemTimeOfNextTick )
			, mTimeScale( rhs.mTimeScale )
			, mQueuedTimeScale( rhs.mQueuedTimeScale )
			, mTick( rhs.mTick )
		{}

		//------------------------------------------------------------------------------
		TimeServer& TimeServer::operator=(const TimeServer& rhs)
		{
			mTime = rhs.mTime;
			mTimeStep = rhs.mTimeStep;
			mTimeScale = rhs.mTimeScale;
			mTick = rhs.mTick;
			mLastTime = rhs.mLastTime;
			mSystemTimeOfNextTick = rhs.mSystemTimeOfNextTick;
			mQueuedTimeScale = rhs.mQueuedTimeScale;

			return *this;
		}

		//------------------------------------------------------------------------------
		void TimeServer::Create(float hz, const TimeAbsolute &timeNow)
		{
			mTime = timeNow;
			mTimeStep = TimeRelative::CreateFromSeconds(1.0f / hz);
			mTimeScale = 1.0f;
			mTick = 0;
			mLastTime = mTime;
			mQueuedTimeScale = mTimeScale;
		}

		//------------------------------------------------------------------------------
		void TimeServer::Reset()
		{
			mTime      = TimeAbsolute::Zero();
			mLastTime  = mTime;

			mTimeScale = 1.0f;
			mTick      = 0;
			mQueuedTimeScale = mTimeScale;
		}

		//------------------------------------------------------------------------------
		void TimeServer::Tick()
		{
			DIA_ASSERT(mTime >= TimeAbsolute::Zero(), "Have not set ther server time properly");

			// If this is the first tick ignore anywaiting
			if (mTick != 0)
			{
				const TimeAbsolute& currentSystemTime = Dia::Core::sSystemClock.CurrentTime();
				// This will cause a block
				if (currentSystemTime < mSystemTimeOfNextTick)
				{ 
					TimeRelative timeToWait = mSystemTimeOfNextTick - currentSystemTime;
					std::chrono::milliseconds dura(timeToWait.AsIntInMilliseconds());
					std::this_thread::sleep_for(dura);
				}
			}

			mSystemTimeOfNextTick = Dia::Core::sSystemClock.CurrentTime() + mTimeStep;

			mLastTime = mTime;

			mTime += mTimeStep * mTimeScale;
			++mTick;

			mTimeScale = mQueuedTimeScale;
		}

		//------------------------------------------------------------------------------
		const TimeAbsolute& TimeServer::GetTime() const
		{
			return mTime;
		}

		//------------------------------------------------------------------------------
		const TimeAbsolute& TimeServer::GetLastTime() const
		{
			return mLastTime;
		}
		
		//------------------------------------------------------------------------------
		const TimeRelative& TimeServer::GetStep() const
		{
			return mTimeStep;
		}

		//------------------------------------------------------------------------------
		TimeRelative TimeServer::GetNextStep() const
		{
			return mTimeStep * mTimeScale;
		}

		//------------------------------------------------------------------------------
		TimeRelative TimeServer::GetLastStep() const
		{
			return mTime - mLastTime;
		}

		//------------------------------------------------------------------------------
		int TimeServer::GetTick() const
		{
			return mTick;
		}

		//------------------------------------------------------------------------------
		void TimeServer::SetTimeScale( float scale )
		{
			mQueuedTimeScale = scale;
		}

		//------------------------------------------------------------------------------
		float TimeServer::GetTimeScale() const
		{
			return mQueuedTimeScale;
		}

		//------------------------------------------------------------------------------
		void TimeServer::AdjustTimeScale( float adjScale )
		{
			mQueuedTimeScale += adjScale;
		}
	}
}