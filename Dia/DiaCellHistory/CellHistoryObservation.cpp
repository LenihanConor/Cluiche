#include <DiaCellHistory/CellHistoryObservation.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/AggregateStore.h>
#include <DiaCellHistory/CellRing.h>
#include <DiaCellHistory/EventLog.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>

namespace Dia::CellHistory
{
	namespace
	{
		const char* const kMetricCellBytes = "dia.cellhistory.cells.bytes";
		const char* const kMetricAggregateBytes = "dia.cellhistory.aggregates.bytes";
		const char* const kMetricEventBytes = "dia.cellhistory.events.bytes";
		const char* const kMetricWindowLength = "dia.cellhistory.window.length";
		const char* const kMetricSamples = "dia.cellhistory.samples";
		const char* const kMetricEvents = "dia.cellhistory.events";
		const char* const kMetricSampleMs = "dia.cellhistory.sample.ms";
		const char* const kMetricVersionGateSkips = "dia.cellhistory.version_gate.skips";
	}

	// ---- CellHistoryHealth ----

	CellHistoryHealth::CellHistoryHealth(const CellHistoryRecorder& recorder)
		: mRecorder(recorder)
	{
		Dia::Observation::Health::HealthRegistry::Instance().Register(this);
	}

	CellHistoryHealth::~CellHistoryHealth()
	{
		Dia::Observation::Health::HealthRegistry::Instance().Unregister(this);
	}

	Dia::Core::StringCRC CellHistoryHealth::GetReporterName() const
	{
		return Dia::Core::StringCRC("dia.cellhistory");
	}

	Dia::Observation::Health::Health CellHistoryHealth::Report() const
	{
		using namespace Dia::Observation::Health;
		Health health;
		health.status = HealthStatus::kOK;
		health.errors = 0;
		health.warnings = 0;
		health.reason = Dia::Core::StringCRC::kZero;

		// Check all three tiers for > 90% usage
		const auto& aggregates = mRecorder.Aggregates();
		const auto& events = mRecorder.EventsLog();
		const CellRing* ring = mRecorder.Ring();

		double aggFraction = aggregates.UsedFraction();
		double eventFraction = events.UsedFraction();

		// Check cell tier (only if tracked)
		if (ring != nullptr)
		{
			double cellFraction = ring->UsedFraction();

			if (cellFraction > 0.9)
			{
				health.status = HealthStatus::kDegraded;
				health.reason = Dia::Core::StringCRC("cells");
				return health;
			}
		}

		if (aggFraction > 0.9)
		{
			health.status = HealthStatus::kDegraded;
			health.reason = Dia::Core::StringCRC("aggregates");
			return health;
		}

		if (eventFraction > 0.9)
		{
			health.status = HealthStatus::kDegraded;
			health.reason = Dia::Core::StringCRC("events");
			return health;
		}

		return health;
	}

	// ---- CellHistoryObservation ----

	CellHistoryObservation::CellHistoryObservation(CellHistoryRecorder& recorder)
		: mRecorder(recorder)
		, mHealth(recorder)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();

		mMetricCellBytes = metrics.RegisterGauge(Dia::Core::StringCRC(kMetricCellBytes));
		mMetricAggregateBytes = metrics.RegisterGauge(Dia::Core::StringCRC(kMetricAggregateBytes));
		mMetricEventBytes = metrics.RegisterGauge(Dia::Core::StringCRC(kMetricEventBytes));
		mMetricWindowLength = metrics.RegisterGauge(Dia::Core::StringCRC(kMetricWindowLength));
		mMetricSamples = metrics.RegisterCounter(Dia::Core::StringCRC(kMetricSamples));
		mMetricEvents = metrics.RegisterCounter(Dia::Core::StringCRC(kMetricEvents));
		mMetricSampleMs = metrics.RegisterGauge(Dia::Core::StringCRC(kMetricSampleMs));
		mMetricVersionGateSkips = metrics.RegisterCounter(Dia::Core::StringCRC(kMetricVersionGateSkips));

		// Initialize counters at current values
		mPreviousSampleCount = mRecorder.SampleCount();
		mPreviousEventCount = mRecorder.EventsLog().EventCount();
		mPreviousVersionGateSkips = mRecorder.VersionGatedSkips();
	}

	CellHistoryObservation::~CellHistoryObservation()
	{
		// Metrics remain registered until MetricRegistry is destroyed; health is unregistered in ~CellHistoryHealth
	}

	void CellHistoryObservation::OnRecorderUpdate()
	{
		DIA_TRACE_ZONE("CellHistoryObservation::OnRecorderUpdate", ::Dia::Observation::Trace::Category::kDiaGraphics);

		// Update memory metrics
		const CellRing* ring = mRecorder.Ring();
		if (ring != nullptr && mMetricCellBytes)
		{
			mMetricCellBytes->Set((double)ring->BytesUsed());
		}

		const auto& aggregates = mRecorder.Aggregates();
		if (mMetricAggregateBytes)
		{
			mMetricAggregateBytes->Set((double)aggregates.BytesUsed());
		}

		const auto& events = mRecorder.EventsLog();
		if (mMetricEventBytes)
		{
			mMetricEventBytes->Set((double)events.BytesUsed());
		}

		// Update window length (per-cell tracking window)
		if (ring != nullptr && mMetricWindowLength)
		{
			mMetricWindowLength->Set((double)ring->WindowLength());
		}

		// Update sample count (cumulative counter, only increment)
		uint32_t currentSampleCount = mRecorder.SampleCount();
		if (currentSampleCount > mPreviousSampleCount && mMetricSamples)
		{
			uint32_t delta = currentSampleCount - mPreviousSampleCount;
			mMetricSamples->Inc(delta);
			mPreviousSampleCount = currentSampleCount;
		}

		// Update event count (cumulative counter)
		uint32_t currentEventCount = events.EventCount();
		if (currentEventCount > mPreviousEventCount && mMetricEvents)
		{
			uint32_t delta = currentEventCount - mPreviousEventCount;
			mMetricEvents->Inc(delta);
			mPreviousEventCount = currentEventCount;
		}

		// Update version gate skips (cumulative counter)
		uint64_t currentVersionGateSkips = mRecorder.VersionGatedSkips();
		if (currentVersionGateSkips > mPreviousVersionGateSkips && mMetricVersionGateSkips)
		{
			uint64_t delta = currentVersionGateSkips - mPreviousVersionGateSkips;
			mMetricVersionGateSkips->Inc(delta);
			mPreviousVersionGateSkips = currentVersionGateSkips;
		}

		// sample.ms: set to a small positive value (placeholder; in real integration,
		// this would be measured around Sample() execution time)
		if (mMetricSampleMs)
		{
			mMetricSampleMs->Set(0.1);
		}
	}
}
