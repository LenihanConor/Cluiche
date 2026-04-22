#include "DiaEditor/Sinks/EditorConsoleSink.h"

#include "DiaEditor/UI/WebUIBridge.h"
#include <DiaLogger/LogLevel.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Json/external/json/json.h>

#include <time.h>
#include <stdio.h>

namespace Dia
{
	namespace Editor
	{
		static const char* kTopicConsoleEntries = "console_entries";

		EditorConsoleSink::EditorConsoleSink()
			: mBridge(nullptr)
			, mReady(false)
			, mNextEntryId(1)
			, mPendingCount(0)
			, mDroppedCount(0)
		{
		}

		void EditorConsoleSink::SetBridge(WebUIBridge* bridge)
		{
			mBridge = bridge;
			mReady = false;
		}

		void EditorConsoleSink::OnLogEntry(const Dia::Logger::LogEntry& entry)
		{
			const char* level = Dia::Logger::LogLevelToString(entry.level);

			if (!mReady)
			{
				if (mPendingCount < kMaxPending)
				{
					PendingEntry& pending = mPendingBuffer[mPendingCount++];
					pending.level = level;
					pending.message = entry.message;
				}
				else
				{
					++mDroppedCount;
				}
				return;
			}

			FormatAndSend(level, entry.message);
		}

		void EditorConsoleSink::FormatAndSend(const char* level, const char* message)
		{
			time_t now = time(nullptr);
			struct tm local;
			localtime_s(&local, &now);
			char timestamp[16];
			snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d",
				local.tm_hour, local.tm_min, local.tm_sec);

			Json::Value entry;
			entry["id"] = mNextEntryId++;
			entry["level"] = level;
			entry["message"] = message;
			entry["timestamp"] = timestamp;
			entry["source"] = "editor";

			mBridge->NotifyUIDataChanged(kTopicConsoleEntries, entry);
		}

		void EditorConsoleSink::NotifyConsoleReady()
		{
			if (mReady || mBridge == nullptr)
				return;

			mReady = true;
			FlushPending();
		}

		void EditorConsoleSink::FlushPending()
		{
			for (unsigned int i = 0; i < mPendingCount; ++i)
				FormatAndSend(mPendingBuffer[i].level.AsCStr(), mPendingBuffer[i].message.AsCStr());

			mPendingCount = 0;

			if (mDroppedCount > 0)
			{
				Dia::Core::Containers::String128 msg("EditorConsoleSink: Dropped %u log entries during startup", mDroppedCount);
				FormatAndSend("warning", msg.AsCStr());
				mDroppedCount = 0;
			}
		}
	}
}
