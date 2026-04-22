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
	}
}
