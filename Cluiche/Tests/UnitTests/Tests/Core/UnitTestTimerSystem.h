#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

#include <DiaCore/Timer/TimerSystem.h>

namespace UnitTests
{
	class UnitTestTimerSystem: public UnitTestCore
	{
	public:
		UnitTestTimerSystem(const Dia::Core::Containers::String32& name);
		UnitTestTimerSystem(void);
		
		void DoStart();
		void DoTest();

	private:
		int mNumberFrames;
		Dia::Core::TimerSystem mTestTimer;
	};
}