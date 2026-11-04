#include <DiaCellHistory/ScrubObservation.h>
#include <DiaCellHistory/HistoryViewState.h>
#include <DiaCellHistory/TimelineModel.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Profile/DiaProfile.h>

namespace Dia::CellHistory
{
	namespace
	{
		const char* const kMetricViewChanges = "dia.cellhistory.view_changes";
		const char* const kMetricScrubSeconds = "dia.cellhistory.scrub_seconds";
		const char* const kMetricTimelineRebuildMs = "dia.cellhistory.timeline_rebuild_ms";
	}

	// ---- ScrubObservation ----

	ScrubObservation::ScrubObservation()
		: mLastSeenEpoch(0)
		, mIsInScrubMode(false)
		, mAccumulatedScrubSeconds(0.0)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

		mMetricViewChanges = metrics.RegisterCounter(Dia::Core::StringCRC(kMetricViewChanges));
		mMetricScrubSeconds = metrics.RegisterGauge(Dia::Core::StringCRC(kMetricScrubSeconds));
		mMetricTimelineRebuildMs = metrics.RegisterGauge(Dia::Core::StringCRC(kMetricTimelineRebuildMs));

		// Initialize
		if (mMetricScrubSeconds)
		{
			mMetricScrubSeconds->Set(0.0);
		}
		if (mMetricTimelineRebuildMs)
		{
			mMetricTimelineRebuildMs->Set(0.0);
		}
	}

	ScrubObservation::~ScrubObservation()
	{
		// Metrics remain registered until MetricRegistry is destroyed
	}

	void ScrubObservation::Tick(double dtSeconds)
	{
		// Accumulate time only when in scrub mode
		if (mIsInScrubMode && mMetricScrubSeconds)
		{
			mAccumulatedScrubSeconds += dtSeconds;
			mMetricScrubSeconds->Set(mAccumulatedScrubSeconds);
		}
	}

	void ScrubObservation::OnViewChange(const HistoryViewState& view, const TimelineModel& timeline)
	{
		DIA_PROFILE_SCOPE("cellhistory.view_change", ::Dia::Observation::Profile::Category::kDiaGraphics);

		// Check if the epoch actually changed
		if (view.Epoch() == mLastSeenEpoch)
		{
			return;	// No-op: epoch didn't change
		}

		mLastSeenEpoch = view.Epoch();

		// Increment the counter on real changes
		if (mMetricViewChanges)
		{
			mMetricViewChanges->Inc();
		}

		// Track scrub mode state
		bool wasScrubbing = mIsInScrubMode;
		mIsInScrubMode = !view.IsLive();

		// If we just went live, reset the accumulated scrub seconds
		if (wasScrubbing && !mIsInScrubMode && mMetricScrubSeconds)
		{
			mAccumulatedScrubSeconds = 0.0;
			mMetricScrubSeconds->Set(0.0);
		}
	}

	void ScrubObservation::OnTimelineRebuild(const TimelineModel& timeline)
	{
		DIA_PROFILE_SCOPE("cellhistory.timeline_rebuild", ::Dia::Observation::Profile::Category::kDiaGraphics);

		if (mMetricTimelineRebuildMs)
		{
			mMetricTimelineRebuildMs->Set(timeline.LastBuildMs());
		}
	}
}
