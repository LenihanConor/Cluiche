#pragma once

// QueryObservation: metrics for the Feature 5 catch-up queries (HistoryQueries). Registered in the constructor with the global
// MetricRegistry (idempotent per name; no unregister, like ExportObservation).
//
// Metrics (counters; successful replies only, errors are not counted):
//   - dia.cellhistory.query_meta / query_aggregates / query_events / query_cells: replies served per query kind
//   - dia.cellhistory.query_bytes: serialised bytes served by all four
//
// Same-PU contract (SimPU); no locks.

#include <stdint.h>

namespace Dia::Observation::Metric { class Counter; }

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

	private:
		Dia::Observation::Metric::Counter*	mByKind[4] = {};
		Dia::Observation::Metric::Counter*	mBytes = nullptr;
	};
}
