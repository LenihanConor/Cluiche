#include "DiaLogger/LogLevel.h"

#include <string.h>

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

		LogLevel LogLevelFromString(const char* str, LogLevel defaultLevel)
		{
			if (str == nullptr) return defaultLevel;

			if (strcmp(str, "trace") == 0)   return LogLevel::kTrace;
			if (strcmp(str, "debug") == 0)   return LogLevel::kDebug;
			if (strcmp(str, "info") == 0)    return LogLevel::kInfo;
			if (strcmp(str, "warning") == 0) return LogLevel::kWarning;
			if (strcmp(str, "error") == 0)   return LogLevel::kError;

			return defaultLevel;
		}
	}
}
