#include "DiaLogger/ThreadLogBuffer.h"

#include <string.h>

namespace Dia
{
	namespace Logger
	{
		ThreadLogBuffer::ThreadLogBuffer()
			: mWriteIndex(0)
			, mReadIndex(0)
		{
		}

		void ThreadLogBuffer::Push(const LogEntry& entry)
		{
			mEntries[mWriteIndex & (kCapacity - 1)] = entry;
			++mWriteIndex;

			if (PendingCount() > kCapacity)
				mReadIndex = mWriteIndex - kCapacity;
		}

		bool ThreadLogBuffer::Pop(LogEntry& outEntry)
		{
			if (IsEmpty())
				return false;

			outEntry = mEntries[mReadIndex & (kCapacity - 1)];
			++mReadIndex;
			return true;
		}

		unsigned int ThreadLogBuffer::PendingCount() const
		{
			return mWriteIndex - mReadIndex;
		}

		bool ThreadLogBuffer::IsEmpty() const
		{
			return mWriteIndex == mReadIndex;
		}

		void ThreadLogBuffer::Reset()
		{
			mReadIndex = mWriteIndex;
		}
	}
}
