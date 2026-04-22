#include "DiaLogger/DebugOutputSink.h"
#include "DiaLogger/LogLevel.h"

#include <DiaCore/Core/Log.h>
#include <DiaCore/Strings/String1024.h>

namespace Dia
{
	namespace Logger
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

		void DebugOutputSink::OnLogEntry(const LogEntry& entry)
		{
			Dia::Core::Containers::String1024 formatted;
			formatted.Format("%s[%s] %s", LevelTag(entry.level), entry.channel.AsChar(), entry.message);
			Dia::Core::Log::OutputLine(formatted.AsCStr());
		}
	}
}
