#pragma once

#include <DiaObservation/Trace/SpanChunk.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		// Pure builder for the Perf Inspector cross-thread chain (row 6b). Input is recent closed spans
		// (oldest first); output is the ChainData JSON the Perf Inspector UI reads, or an unavailable reply.
		// No globals and no engine state: thread names and edge statistics are injected.

		// Per-stream edge statistics already converted to ms / per second. A field the source could not
		// provide stays has* == false and is left out of the edge.
		struct ChainEdgeStats
		{
			bool   hasBytes     = false;  double bytesPerWrite = 0.0;
			bool   hasRate      = false;  double ratePerSec    = 0.0;
			bool   hasDataAge   = false;  double dataAgeMs     = 0.0;
			bool   hasWriteCost = false;  double writeCostMs   = 0.0;
		};

		struct ChainConfig
		{
			const char* chainName;   // ChainData.name
			const char* endZone;     // span that closes the linked chain on the reading side (it carries the flow link)
			const char* tailZone;    // optional span after the end span in the same trace (may be null)
			const char* edgeStream;  // stream id used for edge stats and the edge label
		};

		// Writes the thread's unit name into buf and returns true, or returns false (fallback "thread <id>").
		typedef std::function<bool(uint32_t threadId, char* buf, size_t bufSize)> ThreadNameFn;
		// Returns false when the stream has no stats at all.
		typedef std::function<bool(const char* stream, ChainEdgeStats& out)> EdgeStatsFn;

		// Returns true and writes {"available":true,"name","stages","edges"} when a linked chain ending at
		// cfg.endZone was found; otherwise returns false and writes {"available":false,"reason":...}.
		// Stage ids are span ids as decimal strings. A writer's direct child spans are included (the longest
		// one per name, so a loop of identical zones shows its hot step).
		bool BuildChainJson(const ChainSpan* spans, uint32_t count, const ChainConfig& cfg,
		                    const ThreadNameFn& threadName, const EdgeStatsFn& edgeStats, std::string& outJson);
	}
} // namespace Observation
} // namespace Dia
