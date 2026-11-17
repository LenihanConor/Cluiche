#pragma once

#include <DiaObservation/Trace/ITraceSink.h>
#include <DiaObservation/Trace/SpanChunk.h>

#include <functional>
#include <memory>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		// ITraceSink that batches closed spans into SpanChunks and hands each full (or aged) chunk to an
		// emit callback. Registered on the Tracer, so OnSpan runs on the tracer's single drain thread and
		// needs no locking of its own; the callback is what crosses the thread boundary (the Application
		// wires it to a "$perfspans" EventStream Send). Spans the Tracer skipped (spanId 0) are ignored.
		class SpanChunkSink : public ITraceSink
		{
		public:
			typedef std::function<void(const SpanChunk&)> EmitFn;

			// A chunk is flushed before adding a span that ended more than kMaxChunkAgeNs after the first
			// span in it, so a quiet period never strands spans for long.
			static constexpr uint64_t kMaxChunkAgeNs = 50ull * 1000ull * 1000ull;

			explicit SpanChunkSink(EmitFn emit);

			void OnSpan(const SpanRecord& span) override;

			// Emits any pending spans now.
			void Flush();

		private:
			EmitFn                     mEmit;
			std::unique_ptr<SpanChunk> mChunk;
			uint64_t                   mFirstEndNs;
		};
	}
} // namespace Observation
} // namespace Dia
