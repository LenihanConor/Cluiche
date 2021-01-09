
#include "UnitTestInterface.h"

#include <DiaCore/Time/SystemClock.h>
#include <DiaCore/Strings/String1024.h>
#include <DiaCore/Core/Log.h>

namespace UnitTests
{
	class ThrowMaxAssertData
	{
	public:
		ThrowMaxAssertData(){};
	};

	UnitTestInterface::UnitTestInterface(const Dia::Core::Containers::String32& name)
		: mName(name)
		, mState(kWaiting)
		, mStartTime(Dia::Core::TimeAbsolute::Zero())
		, mRunTime(Dia::Core::TimeRelative::Zero())
		, mNumberOfAsserts(0)
	{}

	UnitTestInterface::UnitTestInterface(void)
		: mState(kWaiting)
		, mStartTime(Dia::Core::TimeAbsolute::Zero())
		, mRunTime(Dia::Core::TimeRelative::Zero())
		, mNumberOfAsserts(0)
	{}

	UnitTestInterface::~UnitTestInterface(void)
	{}

	void UnitTestInterface::Init()
	{
		mState = kWaiting;
		DoInit();
	}

	void UnitTestInterface::Start(const Dia::Core::SystemClock& clock)
	{
		mStartTime = clock.CurrentTime();
		mNumberOfAsserts = 0;
		
		try
		{
			DoStart();
		}
		catch (ThrowMaxAssertData rockCaught)
		{
			rockCaught = rockCaught;
			Dia::Core::Containers::String1024 output("FAIL TEST: %s, to many asserts", Name());
			Dia::Core::Log::OutputLine(output.AsCStr());

			mState = kFinished;
		}
		mState = kRunning;
	}

	void UnitTestInterface::Test()
	{
		try
		{
			DoTest();
		}
		catch (ThrowMaxAssertData rockCaught)
		{
			rockCaught = rockCaught;
			Dia::Core::Containers::String1024 output("FAIL TEST: %s, to many asserts", Name());
			Dia::Core::Log::OutputLine(output.AsCStr());

			mState = kFinished;
		}
	}

	void UnitTestInterface::Stop(const Dia::Core::SystemClock& clock)
	{
		try
		{
			DoStop();
		}
		catch (ThrowMaxAssertData rockCaught)
		{
			rockCaught = rockCaught;
			Dia::Core::Containers::String1024 output("FAIL TEST: %s, to many asserts", Name());
			Dia::Core::Log::OutputLine(output.AsCStr());

			mState = kFinished;
		}

		Dia::Core::TimeAbsolute endTime = clock.CurrentTime();
		mRunTime = endTime - mStartTime;

		mState = kFinished;
	}

	void UnitTestInterface::Flush()
	{
		DoFlush();
	}

	bool UnitTestInterface::IsFinished()const
	{
		return (mState == kFinished);
	}

	UnitTestInterface::eState UnitTestInterface::State()const
	{
		return mState;
	}

	const char* UnitTestInterface::Name()const
	{
		return mName.AsCStr();
	}

	void UnitTestInterface::RecordFail(const char* pExp, const char* str, const char* file, int line)
	{
		mNumberOfAsserts++;
		Dia::Core::Containers::String1024 output("%s(%d): [%s] %s, %s", file, line, mName.AsCStr(), pExp, str);
		Dia::Core::Log::OutputLine(output.AsCStr());

		if (mNumberOfAsserts == kMaxAsserts)
		{
			ThrowMaxAssertData rockToBeThrown;
			throw rockToBeThrown;
		}
	}

	void UnitTestInterface::DoInit(){};
	void UnitTestInterface::DoStart(){};
	void UnitTestInterface::DoTest(){};
	void UnitTestInterface::DoStop(){};
	void UnitTestInterface::DoFlush(){};
}
