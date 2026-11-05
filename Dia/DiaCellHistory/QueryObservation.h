#pragma once

// QueryObservation: metrics for the Feature 5 catch-up queries (HistoryQueries). Registered in the constructor with the global
// MetricRegistry (idempotent per name; no unregister, like ExportObservation).
//
// Metrics (counters; successful replies only, errors are not counted):
//   - dia.cellhistory.query_meta / query_aggregates / query_events / query_cells: replies served per query kind
//   - dia.cellhistory.query_bytes: serialised bytes served by all four
//   - dia.cellhistory.query_aggregates_slices: aggregates replies that were layer slices (one sample/window did not fit whole)
//   - dia.cellhistory.query_aggregates_strided: aggregates replies to a request with stride > 1
//   - dia.cellhistory.query_max_reply_bytes (gauge): largest single reply served since this object was created
//
// Same-PU contract (SimPU); no locks.

#include <stdint.h>

namespace Dia::Observation::Metric { class Counter; class Gauge; }

namespace Dia::CellHistory
{
	class QueryObservation
	{
	public:
		enum class Kind : uint8_t { Meta = 0, Aggregates = 1, Events = 2, Cells = 3 };

		QueryObservation();
		QueryObservation(const QueryObservation&) = delete;
		QueryObservation& operator=(const QueryObservation&) = delete;

		void OnQuery(Kind kind, uint64_t bytes);
		void OnAggregatesShape(bool sliced, bool strided);

	private:
		Dia::Observation::Metric::Counter*	mByKind[4] = {};
		Dia::Observation::Metric::Counter*	mBytes = nullptr;
		Dia::Observation::Metric::Counter*	mSlices = nullptr;
		Dia::Observation::Metric::Counter*	mStrided = nullptr;
		Dia::Observation::Metric::Gauge*	mMaxReply = nullptr;
		uint64_t							mMaxReplyBytes = 0;
	};
}
