////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleHistory.h
// Description: Fixed-size console command history with an Up/Down cursor.
//              Not thread-safe; owned and used by one thread (Main PU).
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			class ConsoleHistory
			{
			public:
				static const unsigned int kMaxEntries = 50;
				static const unsigned int kMaxEntryLength = 255;	// characters, excluding '\0'

				ConsoleHistory();

				// Store line as the newest entry and Reset() the cursor.
				// nullptr/empty lines and a repeat of the newest entry are not stored.
				// Lines longer than kMaxEntryLength are truncated. Oldest is dropped when full.
				void Add(const char* line);

				// Up: step the cursor one entry older and return it. At the oldest entry it
				// stays there and returns it again. Returns nullptr if the history is empty.
				const char* GetPrevious();

				// Down: step the cursor one entry newer and return it. Stepping past the
				// newest entry returns to the end and yields nullptr (restore the draft).
				const char* GetNext();

				// Move the cursor back to the end (past the newest entry).
				void Reset();

				unsigned int GetCount() const { return mCount; }

				// 0 = oldest. Returns nullptr when index >= GetCount().
				const char* GetEntry(unsigned int index) const;

				void Clear();

			private:
				char mEntries[kMaxEntries][kMaxEntryLength + 1];
				unsigned int mHead;		// slot of the oldest entry
				unsigned int mCount;
				unsigned int mCursor;	// 0..mCount; mCount means "at the end"
			};
		}
	}
}
