#pragma once

#include <DiaCore/Strings/String32.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaCore/Time/TimeAbsolute.h>

namespace Dia
{
	namespace Core
	{
		class SystemClock;
	}
}

namespace UnitTests
{
	class UnitTestInterface
	{
	public:
		static const int kMaxFailRecords = 32;
		
		enum Types
		{
			kInvalid = -1,
			kCore,
			kContainer,
			kMaths,
			kGraphics,
			kNumTypes
		};
		
		enum eState
		{
			kWaiting = 0,
			kRunning,
			kFinished
		};

		UnitTestInterface(const Dia::Core::Containers::String32& name);
		UnitTestInterface(void);
		virtual ~UnitTestInterface(void);

		void Init();
		void Start(const Dia::Core::SystemClock& clock);
		void Test();
		void Stop(const Dia::Core::SystemClock& clock);
		void Flush();

		bool IsFinished()const;
		eState State()const;	
		virtual Types Type()const = 0;
		virtual const char* TypeName()const = 0;
		
		const char* Name()const;

		virtual void DoInit();
		virtual void DoStart();
		virtual void DoTest();
		virtual void DoStop();
		virtual void DoFlush();

		void RecordFail(const char* pExp, const char* str, const char* file, int line);
	protected:
		static const int kMaxAsserts = 20;

		eState mState;
		Dia::Core::Containers::String32 mName;
		Dia::Core::TimeAbsolute mStartTime;
		Dia::Core::TimeRelative mRunTime;
		int mNumberOfAsserts;
	};
}
