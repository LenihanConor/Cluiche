#pragma once

#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		// Compact wire form of one closed span for the framework "$perfspans" EventStream (Perf Inspector
		// row 6b). Only the fields the cross-thread chain view needs. The name is carried as text because
		// the editor shows it; it is truncated to kNameLength - 1 characters.
		struct ChainSpan
		{
			static constexpr uint32_t kNameLength = 48;

			uint64_t traceId;
			uint64_t spanId;
			uint64_t parentSpanId;     // 0 if root span
			uint64_t linkTraceId;      // 0 if no flow link
			uint64_t linkSpanId;       // 0 if no flow link
			uint64_t startSteadyNs;
			uint64_t endSteadyNs;
			uint32_t threadId;
			uint32_t nameCrc;
			char     name[kNameLength];
		};

		// POD payload of "$perfspans": a batch of closed spans, oldest first. Sent from the tracer's drain
		// thread by SpanChunkSink, read by one Main-side reader. About 7 KB.
		struct SpanChunk
		{
			static constexpr uint32_t kMaxSpans = 64;

			uint32_t  count;
			ChainSpan spans[kMaxSpans];
		};

		static_assert(sizeof(SpanChunk) <= 9000, "SpanChunk exceeds the EventStream payload budget");
	}
} // namespace Observation
} // namespace Dia
