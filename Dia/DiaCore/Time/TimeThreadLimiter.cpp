#include "DiaCore/Time/TimeThreadLimiter.h"

#include <chrono>
#include <thread>

namespace Dia
{	
	namespace Core
	{
		//------------------------------------------------------------------------------
		TimeThreadLimiter::TimeThreadLimiter(double hz)
			: mFramesPerMillisecond((1.0 / hz) * 1000)
			, mStart()
			, mEnd()
		{}
	
		//------------------------------------------------------------------------------
		TimeThreadLimiter::TimeThreadLimiter(const TimeThreadLimiter& rhs)
			: mFramesPerMillisecond(rhs.mFramesPerMillisecond)
			, mStart(rhs.mStart)
			, mEnd(rhs.mEnd)
		{}

		//------------------------------------------------------------------------------
		TimeThreadLimiter& TimeThreadLimiter::operator=(const TimeThreadLimiter& rhs)
		{
			mFramesPerMillisecond = rhs.mFramesPerMillisecond;
			mStart = rhs.mStart;
			mEnd = rhs.mEnd;

			return *this;
		}

		//------------------------------------------------------------------------------
		void TimeThreadLimiter::Start()
		{
			mStart = std::chrono::high_resolution_clock::now();
		}

		//------------------------------------------------------------------------------
		void TimeThreadLimiter::Stop()
		{
			mEnd = std::chrono::high_resolution_clock::now();
		}

		//------------------------------------------------------------------------------
		void TimeThreadLimiter::SleepThread()
		{
			long remainingTimeInMilliseconds = RemainingTimeInMilliseconds();

			if (remainingTimeInMilliseconds > 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(remainingTimeInMilliseconds));
			}
		}

		//------------------------------------------------------------------------------
		long TimeThreadLimiter::RemainingTimeInMilliseconds()const
		{
			std::chrono::duration<double, std::milli> elapsedMilliseconds = mEnd - mStart;
			std::chrono::duration<double, std::milli> sleepTime = std::chrono::duration<double, std::milli>(mFramesPerMillisecond) - elapsedMilliseconds;

			return sleepTime.count();
		}
	}
}