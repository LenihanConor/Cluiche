#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Trace/TraceCategory.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		struct SpanRecord
		{
			uint64_t             traceId;
			uint64_t             spanId;
			uint64_t             parentSpanId;    // 0 if root span
			Dia::Core::StringCRC name;
			TraceCategory        category;
			uint64_t             startSteadyNs;
			uint64_t             endSteadyNs;
			uint32_t             threadId;
			Dia::Core::StringCRC scenarioStep;
			uint64_t             linkTraceId;     // 0 if no flow link
			uint64_t             linkSpanId;      // 0 if no flow link
		};
	}
} // namespace Observation
} // namespace Dia
