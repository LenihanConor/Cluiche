////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleLogRing.h
// Description: Logger sink that keeps the last kCapacity log lines in a fixed,
//              thread-safe ring for the debug panel's log view.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
//
// Threading:
//   - OnLogEntry may be called from any thread (normally the Logger drain
//     thread, while the Logger holds its non-recursive registry mutex). It only
//     takes this ring's own mutex and copies data. It must NEVER call Logger or
//     DIA_LOG_* (that would deadlock on the Logger's mutex).
//   - DrainSince / GetTotalWritten / GetCount may be called from any thread.
//
// Registration:
//   Logger::RegisterSink returns void and silently ignores the call once
//   Logger::kMaxSinks (8) sinks exist. Owners should log a probe line after
//   registering and check GetTotalWritten() grows to confirm the ring is live.
//
// Size: about 175 KB; allocate on the heap or as a member, not on the stack.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Threading/Mutex.h>

#include <cstdint>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			struct ConsoleLogLine
			{
				static const unsigned int kMaxMessageLength = 255;	// characters, excluding '\0'

				uint64_t sequence;								// 0-based index over all lines ever written
				uint64_t timestampNs;
				Dia::Observation::Log::LogLevel level;
				Dia::Core::StringCRC channel;					// carries the channel name when known (AsChar), else only the CRC
				char message[kMaxMessageLength + 1];			// truncated
			};

			class ConsoleLogRing : public Dia::Observation::Log::ISink
			{
			public:
				static const unsigned int kCapacity = 500;

				ConsoleLogRing();

				// ISink
				void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override;
				const char* GetName() const override { return "ConsoleLogRing"; }

				////////////////////////////////////////////////////////////////////////////////
				// Copy lines with sequence >= cursor into outLines (oldest first), at most
				// maxLines, and advance cursor past the last copied line.
				//   - Start with cursor = 0.
				//   - If cursor is older than the oldest kept line (ring wrapped), copying
				//     starts at the oldest kept line; the lost lines are skipped.
				//   - If cursor is ahead of GetTotalWritten() it is clamped to it.
				// Returns the number of lines copied.
				////////////////////////////////////////////////////////////////////////////////
				unsigned int DrainSince(uint64_t& cursor, ConsoleLogLine* outLines, unsigned int maxLines) const;

				// Number of lines ever written (next line's sequence).
				uint64_t GetTotalWritten() const;

				// Number of lines currently kept (<= kCapacity).
				unsigned int GetCount() const;

			private:
				ConsoleLogRing(const ConsoleLogRing&) = delete;
				ConsoleLogRing& operator=(const ConsoleLogRing&) = delete;

				mutable Dia::Core::Mutex mMutex;
				ConsoleLogLine mLines[kCapacity];
				uint64_t mTotalWritten;
			};
		}
	}
}
