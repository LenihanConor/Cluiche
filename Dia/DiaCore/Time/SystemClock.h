#pragma once

#include "DiaCore/Time/TimeAbsolute.h"

namespace Dia
{	
	namespace Core
	{
		class SystemClock
		{
		public:
			SystemClock();
			TimeAbsolute CurrentTime()const;
		};

		static const SystemClock sSystemClock;
	}
}

