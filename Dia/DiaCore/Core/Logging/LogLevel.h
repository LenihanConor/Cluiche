#ifndef DIA_LOG_LEVEL_H
#define DIA_LOG_LEVEL_H

namespace Dia
{
	namespace Core
	{
		namespace Logging
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Level
			//
			// Defines severity levels for both logging and assertions.
			// Higher numeric values = more severe.
			//
			// USAGE:
			//   LogLevel level = LogLevel::Warning;
			//   if (level >= LogLevel::Error) { /* handle critical */ }
			//
			// LEVELS:
			//   Trace   - Very detailed diagnostic information (function entry/exit, etc)
			//   Debug   - Detailed debugging information
			//   Info    - General informational messages
			//   Warning - Warnings that don't stop execution
			//   Error   - Errors that are handled but noteworthy
			//   Fatal   - Critical errors that crash/terminate
			//
			// ASSERT LEVELS (map to log levels):
			//   Trace   - Assert that logs but doesn't break
			//   Debug   - Assert that breaks in debug builds only
			//   Warning - Assert that logs warning but continues
			//   Error   - Assert that logs error but continues
			//   Fatal   - Assert that always crashes/terminates
			//---------------------------------------------------------------------------------------------------------------------------------

			enum class LogLevel
			{
				Trace = 0,
				Debug = 1,
				Info = 2,
				Warning = 3,
				Error = 4,
				Fatal = 5
			};

			//---------------------------------------------------------------------------------------------------------------------------------
			// Log Level Utilities
			//---------------------------------------------------------------------------------------------------------------------------------

			// Convert log level to string
			inline const char* LogLevelToString(LogLevel level)
			{
				switch (level)
				{
					case LogLevel::Trace:   return "TRACE";
					case LogLevel::Debug:   return "DEBUG";
					case LogLevel::Info:    return "INFO";
					case LogLevel::Warning: return "WARNING";
					case LogLevel::Error:   return "ERROR";
					case LogLevel::Fatal:   return "FATAL";
					default:                return "UNKNOWN";
				}
			}

			// Convert string to log level
			inline LogLevel StringToLogLevel(const char* str)
			{
				if (strcmp(str, "TRACE") == 0)   return LogLevel::Trace;
				if (strcmp(str, "DEBUG") == 0)   return LogLevel::Debug;
				if (strcmp(str, "INFO") == 0)    return LogLevel::Info;
				if (strcmp(str, "WARNING") == 0) return LogLevel::Warning;
				if (strcmp(str, "ERROR") == 0)   return LogLevel::Error;
				if (strcmp(str, "FATAL") == 0)   return LogLevel::Fatal;
				return LogLevel::Info; // Default
			}

			// Check if level is enabled based on minimum level
			inline bool IsLogLevelEnabled(LogLevel level, LogLevel minLevel)
			{
				return static_cast<int>(level) >= static_cast<int>(minLevel);
			}
		}
	}
}

#endif // DIA_LOG_LEVEL_H
