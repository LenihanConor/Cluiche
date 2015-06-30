#include "DiaCore/Timer/TimeThreadLimiter.h"

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
			Dia::Core::TimeRelative remainingTime = RemainingTime();

			if (remainingTime > Dia::Core::TimeRelative::Zero())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(remainingTime.AsIntInMilliseconds()));
			}
		}

		//------------------------------------------------------------------------------
		Dia::Core::TimeRelative TimeThreadLimiter::RemainingTime()const
		{
			std::chrono::duration<double, std::milli> elapsedMilliseconds = mEnd - mStart;
			std::chrono::duration<double, std::milli> sleepTime = std::chrono::duration<double, std::milli>(mFramesPerMillisecond) - elapsedMilliseconds;

			return Dia::Core::TimeRelative::CreateFromMilliseconds(static_cast<float>(sleepTime.count()));
		}
	}
}