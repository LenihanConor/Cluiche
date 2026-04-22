#include "DiaLogger/LogLevel.h"

namespace Dia
{
	namespace Logger
	{
		const char* LogLevelToString(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::kTrace:   return "trace";
			case LogLevel::kDebug:   return "debug";
			case LogLevel::kInfo:    return "info";
			case LogLevel::kWarning: return "warning";
			case LogLevel::kError:   return "error";
			}
			return "unknown";
		}
	}
}
