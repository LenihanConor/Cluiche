#pragma once

#include "DiaPipelineEditor/PipelineEvent.h"
#include <DiaCore/Architecture/Observer.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cstdint>
#include <cstdio>

namespace Dia
{
	namespace PipelineEditor
	{
		class PipelineLogTailer
		{
		public:
			PipelineLogTailer();
			~PipelineLogTailer();

			void Initialize(const char* logPath);
			void Shutdown();

			void Poll();

			bool IsRunInProgress() const;
			const RunSummary& GetCurrentRunSummary() const;

			int GetEventCount() const;
			const PipelineEvent& GetEvent(int index) const;

			void RegisterObserver(Dia::Core::Observer* observer);
			void UnregisterObserver(Dia::Core::Observer* observer);

			enum NotificationMessage
			{
				kNewEvents = 1,
				kRunStarted = 2,
				kRunCompleted = 3,
				kRunInterrupted = 4
			};

		private:
			static constexpr float kInterruptedTimeoutSec = 5.0f;
			static constexpr int kMaxEvents = 10000;
			static constexpr size_t kStringPoolSize = 64 * 1024;

			char mLogPath[512];
			long long mLastReadPos;
			uint64_t mLastFileSize;
			float mLastEventTime;
			bool mRunInProgress;
			bool mInterruptedFired;

			RunSummary mCurrentRun;
			Dia::Core::Containers::DynamicArrayC<PipelineEvent, 256> mEvents;
			int mUnmatchedStartedCount;
			Dia::Core::ObserverSubject mObserverSubject;

			char mStringPool[kStringPoolSize];
			size_t mStringPoolCursor;

			char mLineBuffer[4096];
			size_t mLineBufferLen;

			const char* AllocString(const char* src, size_t len);
			void ClearStringPool();
			void ResetRunState();
			void ProcessLine(const char* line, size_t len);
			void CheckInterrupted(float currentTime);
		};
	}
}
