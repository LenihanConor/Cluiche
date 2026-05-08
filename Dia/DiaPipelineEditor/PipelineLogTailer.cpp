#include "DiaPipelineEditor/PipelineLogTailer.h"
#include "DiaPipelineEditor/Internal/NdjsonLineParser.h"
#include <DiaLogger/DiaLog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace PipelineEditor
	{
		static const Dia::Core::StringCRC kEventOnRunStarted("OnRunStarted");
		static const Dia::Core::StringCRC kEventOnRunCompleted("OnRunCompleted");
		static const Dia::Core::StringCRC kEventOnRunFailed("OnRunFailed");
		static const Dia::Core::StringCRC kEventOnStageStarted("OnStageStarted");
		static const Dia::Core::StringCRC kEventOnStageCompleted("OnStageCompleted");
		static const Dia::Core::StringCRC kEventOnStageFailed("OnStageFailed");
		static const Dia::Core::StringCRC kEventOnStepStarted("OnStepStarted");
		static const Dia::Core::StringCRC kEventOnStepCompleted("OnStepCompleted");
		static const Dia::Core::StringCRC kEventOnStepFailed("OnStepFailed");
		static const Dia::Core::StringCRC kEventOnLogLine("OnLogLine");

		PipelineLogTailer::PipelineLogTailer()
			: mLastReadPos(0)
			, mLastFileSize(0)
			, mLastEventTime(0.0f)
			, mRunInProgress(false)
			, mInterruptedFired(false)
			, mUnmatchedStartedCount(0)
			, mStringPoolCursor(0)
			, mLineBufferLen(0)
		{
			mLogPath[0] = '\0';
			memset(mStringPool, 0, sizeof(mStringPool));
			memset(mLineBuffer, 0, sizeof(mLineBuffer));
		}

		PipelineLogTailer::~PipelineLogTailer()
		{
		}

		void PipelineLogTailer::Initialize(const char* logPath)
		{
			strncpy_s(mLogPath, sizeof(mLogPath), logPath, _TRUNCATE);
			ResetRunState();
		}

		void PipelineLogTailer::Shutdown()
		{
			mLogPath[0] = '\0';
			ResetRunState();
		}

		void PipelineLogTailer::Poll()
		{
			if (mLogPath[0] == '\0')
				return;

			// Use CreateFileA with FILE_SHARE_READ|WRITE so we can read
			// while the Python subprocess holds the file open for writing.
			HANDLE hFile = CreateFileA(
				mLogPath,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);

			if (hFile == INVALID_HANDLE_VALUE)
				return;

			LARGE_INTEGER liSize;
			if (!GetFileSizeEx(hFile, &liSize))
			{
				CloseHandle(hFile);
				return;
			}
			long long fileSize = liSize.QuadPart;

			if (fileSize < mLastReadPos)
			{
				ResetRunState();
			}

			if (fileSize == mLastReadPos)
			{
				CloseHandle(hFile);
				LARGE_INTEGER freq, now;
				QueryPerformanceFrequency(&freq);
				QueryPerformanceCounter(&now);
				float currentTime = static_cast<float>(now.QuadPart) / static_cast<float>(freq.QuadPart);
				CheckInterrupted(currentTime);
				return;
			}

			LARGE_INTEGER liSeek;
			liSeek.QuadPart = mLastReadPos;
			SetFilePointerEx(hFile, liSeek, NULL, FILE_BEGIN);

			long long bytesToRead = fileSize - mLastReadPos;
			bool hasNewEvents = false;

			while (bytesToRead > 0)
			{
				size_t spaceInBuffer = sizeof(mLineBuffer) - mLineBufferLen - 1;
				if (spaceInBuffer == 0)
				{
					DIA_LOG_WARNING("PipelineEditor", "Line buffer overflow — skipping partial line");
					mLineBufferLen = 0;
					break;
				}

				DWORD toRead = (static_cast<DWORD>(bytesToRead) < static_cast<DWORD>(spaceInBuffer))
					? static_cast<DWORD>(bytesToRead)
					: static_cast<DWORD>(spaceInBuffer);

				DWORD bytesRead = 0;
				if (!ReadFile(hFile, mLineBuffer + mLineBufferLen, toRead, &bytesRead, NULL) || bytesRead == 0)
					break;

				mLineBufferLen += bytesRead;
				mLineBuffer[mLineBufferLen] = '\0';
				bytesToRead -= static_cast<long long>(bytesRead);

				char* start = mLineBuffer;
				char* newline = nullptr;

				while ((newline = strchr(start, '\n')) != nullptr)
				{
					size_t lineLen = static_cast<size_t>(newline - start);
					if (lineLen > 0 && start[lineLen - 1] == '\r')
						lineLen--;

					if (lineLen > 0)
					{
						ProcessLine(start, lineLen);
						hasNewEvents = true;
					}

					start = newline + 1;
				}

				size_t remaining = mLineBufferLen - static_cast<size_t>(start - mLineBuffer);
				if (remaining > 0)
					memmove(mLineBuffer, start, remaining);
				mLineBufferLen = remaining;
			}

			mLastReadPos = fileSize;
			CloseHandle(hFile);

			if (hasNewEvents)
			{
				LARGE_INTEGER freq, now;
				QueryPerformanceFrequency(&freq);
				QueryPerformanceCounter(&now);
				mLastEventTime = static_cast<float>(now.QuadPart) / static_cast<float>(freq.QuadPart);

				mObserverSubject.NotifyObservers(kNewEvents);
			}
		}

		bool PipelineLogTailer::IsRunInProgress() const
		{
			return mRunInProgress;
		}

		const RunSummary& PipelineLogTailer::GetCurrentRunSummary() const
		{
			return mCurrentRun;
		}

		int PipelineLogTailer::GetEventCount() const
		{
			return static_cast<int>(mEvents.Size());
		}

		const PipelineEvent& PipelineLogTailer::GetEvent(int index) const
		{
			return mEvents[static_cast<unsigned int>(index)];
		}

		void PipelineLogTailer::RegisterObserver(Dia::Core::Observer* observer)
		{
			mObserverSubject.AttachToObserver(observer);
		}

		void PipelineLogTailer::UnregisterObserver(Dia::Core::Observer* observer)
		{
			mObserverSubject.DetachFromObserver(observer);
		}

		const char* PipelineLogTailer::AllocString(const char* src, size_t len)
		{
			if (src == nullptr || len == 0)
				return nullptr;

			if (mStringPoolCursor + len + 1 > kStringPoolSize)
			{
				DIA_LOG_WARNING("PipelineEditor", "String pool exhausted — cannot allocate %zu bytes", len);
				return nullptr;
			}

			char* dest = mStringPool + mStringPoolCursor;
			memcpy(dest, src, len);
			dest[len] = '\0';
			mStringPoolCursor += len + 1;
			return dest;
		}

		void PipelineLogTailer::ClearStringPool()
		{
			mStringPoolCursor = 0;
		}

		void PipelineLogTailer::ResetRunState()
		{
			mEvents.RemoveAll();
			mCurrentRun = RunSummary{};
			mRunInProgress = false;
			mInterruptedFired = false;
			mUnmatchedStartedCount = 0;
			mLastReadPos = 0;
			mLastFileSize = 0;
			mLastEventTime = 0.0f;
			mLineBufferLen = 0;
			ClearStringPool();
		}

		void PipelineLogTailer::ProcessLine(const char* line, size_t len)
		{
			Internal::ParsedLine parsed;
			if (!Internal::ParseNdjsonLine(line, len, parsed))
			{
				DIA_LOG_WARNING("PipelineEditor", "Malformed JSON line — skipping");
				return;
			}

			PipelineEvent& evt = parsed.event;

			// Allocate display text strings into pool
			// The parser left raw JSON text — we need to extract error/detail/level from JSON again
			// NdjsonLineParser doesn't extract these; do it here with a second parse for display fields
			{
				Json::Reader reader;
				Json::Value root;
				if (reader.parse(line, line + len, root, false))
				{
					if (root.isMember("error"))
					{
						const char* str = root["error"].asCString();
						evt.error = AllocString(str, strlen(str));
					}
					if (root.isMember("detail"))
					{
						const char* str = root["detail"].asCString();
						evt.detail = AllocString(str, strlen(str));
					}
					if (root.isMember("level"))
					{
						const char* str = root["level"].asCString();
						evt.level = AllocString(str, strlen(str));
					}
					if (root.isMember("message"))
					{
						const char* str = root["message"].asCString();
						if (evt.detail == nullptr)
							evt.detail = AllocString(str, strlen(str));
					}
				}
			}

			if (evt.eventType == kEventOnRunStarted)
			{
				ClearStringPool();
				mEvents.RemoveAll();
				mRunInProgress = true;
				mInterruptedFired = false;
				mUnmatchedStartedCount = 0;

				mCurrentRun = RunSummary{};
				mCurrentRun.startTimestamp = evt.timestampSec;

				if (parsed.hasTarget)
					mCurrentRun.target = Dia::Core::StringCRC(parsed.target);
				if (parsed.hasConfig)
					mCurrentRun.config = Dia::Core::StringCRC(parsed.config);

				if (parsed.hasSchema && strcmp(parsed.schema, "dia.output.v1") != 0)
				{
					DIA_LOG_WARNING("PipelineEditor", "Unknown NDJSON schema: %s", parsed.schema);
				}

				mObserverSubject.NotifyObservers(kRunStarted);
			}
			else if (evt.eventType == kEventOnStageStarted)
			{
				mUnmatchedStartedCount++;
			}
			else if (evt.eventType == kEventOnStageCompleted)
			{
				if (mUnmatchedStartedCount > 0)
					mUnmatchedStartedCount--;
				mCurrentRun.passCount++;
			}
			else if (evt.eventType == kEventOnStageFailed)
			{
				if (mUnmatchedStartedCount > 0)
					mUnmatchedStartedCount--;
				mCurrentRun.failCount++;
			}
			// Step events: appended to mEvents and forwarded to observers, but do NOT
			// touch mUnmatchedStartedCount — interrupt detection remains stage-based.
			// (No else-if needed; fall through to the mEvents.Add below.)

			else if (evt.eventType == kEventOnRunCompleted || evt.eventType == kEventOnRunFailed)
			{
				mRunInProgress = false;
				if (evt.durationMs >= 0)
					mCurrentRun.totalDurationMs = evt.durationMs;

				mObserverSubject.NotifyObservers(kRunCompleted);
			}

			if (static_cast<int>(mEvents.Size()) < kMaxEvents)
			{
				mEvents.Add(evt);
			}
			else
			{
				static bool sWarned = false;
				if (!sWarned)
				{
					DIA_LOG_WARNING("PipelineEditor", "Event cap (%d) reached — further events discarded", kMaxEvents);
					sWarned = true;
				}
			}
		}

		void PipelineLogTailer::CheckInterrupted(float currentTime)
		{
			if (mInterruptedFired)
				return;

			if (!mRunInProgress)
				return;

			if (mUnmatchedStartedCount <= 0)
				return;

			if (mLastEventTime <= 0.0f)
				return;

			if ((currentTime - mLastEventTime) > kInterruptedTimeoutSec)
			{
				mCurrentRun.interrupted = true;
				mRunInProgress = false;
				mInterruptedFired = true;
				mObserverSubject.NotifyObservers(kRunInterrupted);
			}
		}
	}
}
