#include "DiaCore/Time/TimeServer.h"

#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		//------------------------------------------------------------------------------
		TimeServer::TimeServer()
			: mTime( TimeAbsolute::MinimumTime() )
			, mTimeStep( TimeRelative::MinimumTime() )
			, mLastTime( TimeAbsolute::Zero() )
			, mTimeScale(1.0f)
			, mQueuedTimeScale( 0.0f )
			, mTick(0)
			, mIsPaused(false)
		{}

		//------------------------------------------------------------------------------
		TimeServer::TimeServer( float hz, const TimeAbsolute &timeNow )
			: mTime( timeNow )
			, mTimeStep( TimeRelative::CreateFromSeconds(1.0f / hz) )
			, mLastTime( mTime )
			, mTimeScale(1.0f)
			, mQueuedTimeScale( mTimeScale )
			, mTick(0)
			, mIsPaused(false)
		{
			DIA_ASSERT(mTimeStep > Dia::Core::TimeRelative::Zero(), "step is too small" );
		}

		//------------------------------------------------------------------------------
		TimeServer::TimeServer(const TimeServer& rhs)
			: mTime( rhs.mTime )
			, mTimeStep( rhs.mTimeStep )
			, mLastTime( rhs.mLastTime )
			, mTimeScale( rhs.mTimeScale )
			, mQueuedTimeScale( rhs.mQueuedTimeScale )
			, mTick( rhs.mTick )
			, mIsPaused( rhs.mIsPaused )
		{}

		//------------------------------------------------------------------------------
		TimeServer& TimeServer::operator=(const TimeServer& rhs)
		{
			mTime = rhs.mTime;
			mTimeStep = rhs.mTimeStep;
			mTimeScale = rhs.mTimeScale;
			mTick = rhs.mTick;
			mLastTime = rhs.mLastTime;
			mQueuedTimeScale = rhs.mQueuedTimeScale;
			mIsPaused = rhs.mIsPaused;

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

			if (mIsPaused)
			{
				return;
			}

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

		//------------------------------------------------------------------------------
		void TimeServer::Pause()
		{
			mIsPaused = true;
		}

		//------------------------------------------------------------------------------
		void TimeServer::Resume()
		{
			mIsPaused = false;
		}

		//------------------------------------------------------------------------------
		bool TimeServer::IsPaused() const
		{
			return mIsPaused;
		}

		//------------------------------------------------------------------------------
		void TimeServer::Step(TimeRelative step)
		{
			mLastTime = mTime;
			mTime += step;
			++mTick;
		}

		//------------------------------------------------------------------------------
		void TimeServer::AdvanceTo(TimeAbsolute target)
		{
			if (target <= mTime)
			{
				return;
			}

			mLastTime = mTime;
			mTime = target;
			++mTick;
		}
	}
}
