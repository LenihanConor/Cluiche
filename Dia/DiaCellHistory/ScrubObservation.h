#pragma once

// ScrubObservation: Observation (metrics, traces, profiling) for scrubbing and timeline building in Feature 2.
// Registers and maintains metrics (dia.cellhistory.* prefix) with MetricRegistry, traces/profiles
// around view state changes and timeline rebuilds.
//
// Metrics:
//   - dia.cellhistory.view_changes (counter): increments on each view-mode or view-epoch change
//   - dia.cellhistory.scrub_seconds (gauge): accumulated seconds spent in non-live (scrub) mode
//   - dia.cellhistory.timeline_rebuild_ms (gauge): last timeline build duration from TimelineModel::LastBuildMs()
//
// Lifecycle: construct with default or injectable clock, call Tick(dtSeconds) on frame, call OnViewChange()
// when the view state epoch changes, and OnTimelineRebuild() when the timeline builds.
// Metrics are registered in constructor and unregistered on destruction.
//
// Thread safety: same-PU as the recorder (SimPU); no locks.

#include <cstdint>

namespace Dia::CellHistory
{
	class HistoryViewState;
	class TimelineModel;
}

namespace Dia::Observation::Metric { class Gauge; class Counter; }

namespace Dia::CellHistory
{
	class ScrubObservation
	{
	public:
		ScrubObservation();
		~ScrubObservation();
		ScrubObservation(const ScrubObservation&) = delete;
		ScrubObservation& operator=(const ScrubObservation&) = delete;

		// Call once per frame with time delta in seconds
		void Tick(double dtSeconds);

		// Call when the view state changes (checks epoch to detect real changes)
		void OnViewChange(const HistoryViewState& view, const TimelineModel& timeline);

		// Call after timeline.Build() to update the rebuild time metric
		void OnTimelineRebuild(const TimelineModel& timeline);

	private:
		// Metric handles (non-owning; registered with global MetricRegistry)
		Dia::Observation::Metric::Counter* mMetricViewChanges = nullptr;
		Dia::Observation::Metric::Gauge* mMetricScrubSeconds = nullptr;
		Dia::Observation::Metric::Gauge* mMetricTimelineRebuildMs = nullptr;

		// State tracking
		uint64_t mLastSeenEpoch = 0;
		bool mIsInScrubMode = false;
		double mAccumulatedScrubSeconds = 0.0;
	};
}
