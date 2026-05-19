#pragma once

#include <DiaObservation/Log/LogLevel.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Log
	{
		struct LogEntry
		{
			LogLevel level;
			Dia::Core::StringCRC channel;
			char message[1024];
			uint64_t timestampNs;
			uint32_t threadId;
			Dia::Core::StringCRC scenarioStep;
		};
	}
} // namespace Observation
} // namespace Dia
