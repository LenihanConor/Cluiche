#pragma once

#include <DiaLogger/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Logger
	{
		struct LogEntry
		{
			LogLevel level;
			Dia::Core::StringCRC channel;
			char message[1024];
		};
	}
}
