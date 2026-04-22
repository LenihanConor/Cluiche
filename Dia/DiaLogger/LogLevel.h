#pragma once

namespace Dia
{
	namespace Logger
	{
		enum class LogLevel : unsigned char
		{
			kTrace,
			kDebug,
			kInfo,
			kWarning,
			kError
		};

		const char* LogLevelToString(LogLevel level);
		LogLevel LogLevelFromString(const char* str, LogLevel defaultLevel = LogLevel::kInfo);
	}
}
