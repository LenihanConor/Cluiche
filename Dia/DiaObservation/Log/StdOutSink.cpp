#include "DiaObservation/Log/StdOutSink.h"
#include "DiaObservation/Log/LogLevel.h"

#include <cstdio>

namespace Dia
{
	namespace Observation { namespace Log
	{
		static const char* LevelTag(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::kTrace:   return "[TRACE]";
			case LogLevel::kDebug:   return "[DEBUG]";
			case LogLevel::kInfo:    return "[INFO]";
			case LogLevel::kWarning: return "[WARNING]";
			case LogLevel::kError:   return "[ERROR]";
			}
			return "[???]";
		}

		void StdOutSink::OnLogEntry(const LogEntry& entry)
		{
			std::printf("%s[%s] %s\n", LevelTag(entry.level), entry.channel.AsChar(), entry.message);
			std::fflush(stdout);
		}
	}
} // namespace Observation
} // namespace Dia
