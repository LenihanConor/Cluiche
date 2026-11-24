#pragma once

// SnapshotObservation: metrics for Feature 3 (snapshots). Registered in the constructor with the global MetricRegistry
// (idempotent per name; the registry has no unregister, like ScrubObservation / CellHistoryObservation; no MetricInfo is
// attached, matching Features 1 and 2).
//
// Metrics:
//   - dia.cellhistory.snapshots (gauge): ready snapshot records known to the manager (updated on take, delete and list scans)
//   - dia.cellhistory.snapshot_take_ms (gauge): last take, capture in Take() plus the cell file write in the store callback
//   - dia.cellhistory.snapshot_restore_ms (gauge): last restore, from the Restore() request to the end of the store callback
//   - dia.cellhistory.snapshot_bytes (gauge): size of the last cell file written
//   - dia.cellhistory.snapshot_failures (counter): refused or failed takes, restores and deletes
//   - dia.cellhistory.snapshot_queries_served (counter): cells.history.snapshot calls
//
// The clock (milliseconds, monotonic) is injectable so tests are deterministic. Same-PU contract (SimPU); no locks.
// The manager and commands take an optional pointer; null means no observation and no cost.

#include <stdint.h>

namespace Dia::Observation::Metric { class Gauge; class Counter; }

namespace Dia::CellHistory
{
	class SnapshotObservation
	{
	public:
		using ClockMsFn = double(*)(void* user);

		explicit SnapshotObservation(ClockMsFn clock = nullptr, void* clockUser = nullptr);
		SnapshotObservation(const SnapshotObservation&) = delete;
		SnapshotObservation& operator=(const SnapshotObservation&) = delete;

		double NowMs() const;

		void SetSnapshotCount(uint32_t readyCount);
		void OnTakeCompleted(double captureMs, double writeMs, uint64_t bytes);
		void OnRestoreCompleted(double elapsedMs);
		void OnFailure();
		void OnQueryServed();

	private:
		ClockMsFn	mClock = nullptr;
		void*		mClockUser = nullptr;
		Dia::Observation::Metric::Gauge*	mSnapshots = nullptr;
		Dia::Observation::Metric::Gauge*	mTakeMs = nullptr;
		Dia::Observation::Metric::Gauge*	mRestoreMs = nullptr;
		Dia::Observation::Metric::Gauge*	mBytes = nullptr;
		Dia::Observation::Metric::Counter*	mFailures = nullptr;
		Dia::Observation::Metric::Counter*	mQueries = nullptr;
	};
}
