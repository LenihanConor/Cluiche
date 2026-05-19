#pragma once

#include <DiaObservation/Log/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Observation { namespace Log
	{
		struct LogEntry
		{
			LogLevel level;
			Dia::Core::StringCRC channel;
			char message[1024];
		};
	}
} // namespace Observation
} // namespace Dia
