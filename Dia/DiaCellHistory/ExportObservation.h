#pragma once

// ExportObservation: metrics for Feature 5 (export). Registered in the constructor with the global MetricRegistry
// (idempotent per name; no unregister, like SnapshotObservation; no MetricInfo attached).
//
// Metrics:
//   - dia.cellhistory.exports (counter): files written successfully
//   - dia.cellhistory.export_ms (gauge): duration of the last successful export
//   - dia.cellhistory.export_bytes (gauge): size of the last successful export
//
// The clock (milliseconds, monotonic) is injectable so tests are deterministic. Same-PU contract (SimPU); no locks.

#include <stdint.h>

namespace Dia::Observation::Metric { class Gauge; class Counter; }

namespace Dia::CellHistory
{
	class ExportObservation
	{
	public:
		using ClockMsFn = double(*)(void* user);

		explicit ExportObservation(ClockMsFn clock = nullptr, void* clockUser = nullptr);
		ExportObservation(const ExportObservation&) = delete;
		ExportObservation& operator=(const ExportObservation&) = delete;

		double NowMs() const;
		void OnExported(double elapsedMs, uint64_t bytes);

	private:
		ClockMsFn	mClock = nullptr;
		void*		mClockUser = nullptr;
		Dia::Observation::Metric::Counter*	mExports = nullptr;
		Dia::Observation::Metric::Gauge*	mMs = nullptr;
		Dia::Observation::Metric::Gauge*	mBytes = nullptr;
	};
}
