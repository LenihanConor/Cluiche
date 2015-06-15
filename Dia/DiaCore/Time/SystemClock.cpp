#include "DiaCore/Time/SystemClock.h"

#include <time.h>
#include <Windows.h>

namespace Dia
{	
	namespace Core
	{
		SystemClock::SystemClock()
		{}

		TimeAbsolute SystemClock::CurrentTime()const
		{
			FILETIME UTCFileTime;
			GetSystemTimeAsFileTime( &UTCFileTime );
		
			ULARGE_INTEGER time;
			time.HighPart	= UTCFileTime.dwHighDateTime;
			time.LowPart	= UTCFileTime.dwLowDateTime;
			
			long long timeResult = time.QuadPart / 10;

			return TimeAbsolute::CreateFromMicroseconds( timeResult );
		}
	}
}