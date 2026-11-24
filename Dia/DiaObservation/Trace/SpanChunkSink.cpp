#include "DiaObservation/Trace/SpanChunkSink.h"

#include <cstring>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		SpanChunkSink::SpanChunkSink(EmitFn emit)
			: mEmit(std::move(emit))
			, mChunk(new SpanChunk())
			, mFirstEndNs(0)
		{
			mChunk->count = 0;
		}

		void SpanChunkSink::Flush()
		{
			if (mChunk->count == 0)
				return;
			if (mEmit)
				mEmit(*mChunk);
			mChunk->count = 0;
		}

		void SpanChunkSink::OnSpan(const SpanRecord& span)
		{
			if (span.spanId == 0)
				return;

			if (mChunk->count > 0 && span.endSteadyNs > mFirstEndNs + kMaxChunkAgeNs)
				Flush();

			if (mChunk->count == 0)
				mFirstEndNs = span.endSteadyNs;

			ChainSpan& out = mChunk->spans[mChunk->count++];
			out.traceId       = span.traceId;
			out.spanId        = span.spanId;
			out.parentSpanId  = span.parentSpanId;
			out.linkTraceId   = span.linkTraceId;
			out.linkSpanId    = span.linkSpanId;
			out.startSteadyNs = span.startSteadyNs;
			out.endSteadyNs   = span.endSteadyNs;
			out.threadId      = span.threadId;
			out.nameCrc       = span.name.Value();
			std::memset(out.name, 0, sizeof(out.name));
			const char* text = span.name.AsChar();
			if (text != nullptr)
				std::strncpy(out.name, text, ChainSpan::kNameLength - 1);

			if (mChunk->count >= SpanChunk::kMaxSpans)
				Flush();
		}
	}
} // namespace Observation
} // namespace Dia
