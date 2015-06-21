#pragma once

#include <chrono>

namespace Dia
{	
	namespace Core
	{
		//==============================================================================
		// CLASS TimeThreadLimiter
		//==============================================================================
		class TimeThreadLimiter
		{
		public:		
			TimeThreadLimiter( double hz );

			TimeThreadLimiter(const TimeThreadLimiter& rhs);
			TimeThreadLimiter& operator=(const TimeThreadLimiter& rhs);

			void					Start();
			void					Stop();
			void					SleepThread();

			long					RemainingTimeInMilliseconds()const;		// If this negative the frame has gone over!
	
		private:
			TimeThreadLimiter();

			double mFramesPerMillisecond;											// What frequency should the thread go at
			std::chrono::time_point<std::chrono::high_resolution_clock> mStart;		// Start frame time
			std::chrono::time_point<std::chrono::high_resolution_clock> mEnd;		// End frame time
		};
	}
}
