#pragma once

#include <DiaLogger/ISink.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String1024.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;

		class EditorConsoleSink : public Dia::Logger::ISink
		{
		public:
			EditorConsoleSink();

			void SetBridge(WebUIBridge* bridge);

			void OnLogEntry(const Dia::Logger::LogEntry& entry) override;
			const char* GetName() const override { return "EditorConsole"; }

			void NotifyConsoleReady();

		private:
			struct PendingEntry
			{
				Dia::Core::Containers::String32 level;
				Dia::Core::Containers::String1024 message;
			};

			void FormatAndSend(const char* level, const char* message);
			void FlushPending();

			WebUIBridge* mBridge;
			bool mReady;
			unsigned int mNextEntryId;

			static const unsigned int kMaxPending = 128;
			PendingEntry mPendingBuffer[kMaxPending];
			unsigned int mPendingCount;
			unsigned int mDroppedCount;
		};
	}
}
