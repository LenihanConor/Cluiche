////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleHistory.cpp
// Description: Fixed-size console command history with an Up/Down cursor.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/ConsoleHistory.h>

#include <cstring>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			ConsoleHistory::ConsoleHistory()
			{
				Clear();
			}

			void ConsoleHistory::Clear()
			{
				mHead = 0;
				mCount = 0;
				mCursor = 0;
				for (unsigned int i = 0; i < kMaxEntries; ++i)
				{
					mEntries[i][0] = '\0';
				}
			}

			const char* ConsoleHistory::GetEntry(unsigned int index) const
			{
				if (index >= mCount)
				{
					return nullptr;
				}
				return mEntries[(mHead + index) % kMaxEntries];
			}

			void ConsoleHistory::Add(const char* line)
			{
				if (line == nullptr || line[0] == '\0')
				{
					Reset();
					return;
				}

				char truncated[kMaxEntryLength + 1];
				unsigned int length = 0;
				while (length < kMaxEntryLength && line[length] != '\0')
				{
					truncated[length] = line[length];
					++length;
				}
				truncated[length] = '\0';

				if (mCount > 0 && std::strcmp(GetEntry(mCount - 1), truncated) == 0)
				{
					Reset();
					return;
				}

				unsigned int slot;
				if (mCount < kMaxEntries)
				{
					slot = (mHead + mCount) % kMaxEntries;
					++mCount;
				}
				else
				{
					slot = mHead;
					mHead = (mHead + 1) % kMaxEntries;
				}
				std::memcpy(mEntries[slot], truncated, length + 1);
				Reset();
			}

			const char* ConsoleHistory::GetPrevious()
			{
				if (mCount == 0)
				{
					return nullptr;
				}
				if (mCursor > 0)
				{
					--mCursor;
				}
				return GetEntry(mCursor);
			}

			const char* ConsoleHistory::GetNext()
			{
				if (mCursor >= mCount)
				{
					mCursor = mCount;
					return nullptr;
				}
				++mCursor;
				return GetEntry(mCursor);	// nullptr once past the newest
			}

			void ConsoleHistory::Reset()
			{
				mCursor = mCount;
			}
		}
	}
}
