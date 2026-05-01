#pragma once

#include <DiaLogger/LogEntry.h>

namespace Dia
{
	namespace Logger
	{
		class ThreadLogBuffer
		{
		public:
			static const unsigned int kCapacity = 1024;

			ThreadLogBuffer();

			void Push(const LogEntry& entry);
			bool Pop(LogEntry& outEntry);

			unsigned int PendingCount() const;
			bool IsEmpty() const;
			void Reset();

		private:
			LogEntry mEntries[kCapacity];
			unsigned int mWriteIndex;
			unsigned int mReadIndex;
		};
	}
}
