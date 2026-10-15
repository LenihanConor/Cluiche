////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleLogRing.cpp
// Description: Thread-safe ring of the last kCapacity log lines.
//              Never logs: OnLogEntry runs under the Logger's registry mutex.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/ConsoleLogRing.h>

#include <cstring>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			ConsoleLogRing::ConsoleLogRing()
				: mTotalWritten(0)
			{
				for (unsigned int i = 0; i < kCapacity; ++i)
				{
					mLines[i].sequence = 0;
					mLines[i].timestampNs = 0;
					mLines[i].level = Dia::Observation::Log::LogLevel::kInfo;
					mLines[i].message[0] = '\0';
				}
			}

			void ConsoleLogRing::OnLogEntry(const Dia::Observation::Log::LogEntry& entry)
			{
				// No Logger / DIA_LOG_* calls in here (see header).
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);

				ConsoleLogLine& line = mLines[mTotalWritten % kCapacity];
				line.sequence = mTotalWritten;
				line.timestampNs = entry.timestampNs;
				line.level = entry.level;
				line.channel = entry.channel;

				unsigned int length = 0;
				const unsigned int sourceSize = static_cast<unsigned int>(sizeof(entry.message));
				while (length < ConsoleLogLine::kMaxMessageLength && length < sourceSize && entry.message[length] != '\0')
				{
					line.message[length] = entry.message[length];
					++length;
				}
				line.message[length] = '\0';

				++mTotalWritten;
			}

			unsigned int ConsoleLogRing::DrainSince(uint64_t& cursor, ConsoleLogLine* outLines, unsigned int maxLines) const
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);

				const uint64_t oldest = (mTotalWritten > kCapacity) ? (mTotalWritten - kCapacity) : 0;
				if (cursor < oldest)
				{
					cursor = oldest;
				}
				if (cursor > mTotalWritten)
				{
					cursor = mTotalWritten;
				}

				if (outLines == nullptr || maxLines == 0)
				{
					return 0;
				}

				uint64_t available = mTotalWritten - cursor;
				const unsigned int count = (available < maxLines) ? static_cast<unsigned int>(available) : maxLines;
				for (unsigned int i = 0; i < count; ++i)
				{
					outLines[i] = mLines[(cursor + i) % kCapacity];
				}
				cursor += count;
				return count;
			}

			uint64_t ConsoleLogRing::GetTotalWritten() const
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);
				return mTotalWritten;
			}

			unsigned int ConsoleLogRing::GetCount() const
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mMutex);
				return (mTotalWritten > kCapacity) ? kCapacity : static_cast<unsigned int>(mTotalWritten);
			}
		}
	}
}
