#pragma once

#include <DiaObservation/Log/LogEntry.h>

namespace Dia
{
	namespace Observation { namespace Log
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
} // namespace Observation
} // namespace Dia
