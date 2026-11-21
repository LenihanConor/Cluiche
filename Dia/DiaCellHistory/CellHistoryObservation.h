#pragma once

// CellHistoryObservation: Observation (metrics, traces, profiling, health) for CellHistoryRecorder.
// Registers and maintains metrics (dia.cellhistory.* prefix) with MetricRegistry, traces/profiles
// around recorder operations, and provides an IHealthReporter that reports Degraded when any
// tier exceeds 90% of its budget.
//
// Lifecycle: construct with a recorder reference, update via OnRecorderUpdate() after each Sample.
// Metrics are registered in the constructor and automatically updated. Health reporter is registered
// with HealthRegistry on construction and unregistered on destruction.
//
// Thread safety: same-PU as the recorder (SimPU); no locks.

#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Health/IHealthReporter.h>
#include <cstdint>

namespace Dia::CellHistory { class CellHistoryRecorder; }
namespace Dia::Observation::Metric { class Gauge; class Counter; }

namespace Dia::CellHistory
{
	// Health reporter: Degraded when any of the three tiers (cell, aggregate, event) is over 90% of budget.
	class CellHistoryHealth : public Dia::Observation::Health::IHealthReporter
	{
	public:
		explicit CellHistoryHealth(const CellHistoryRecorder& recorder);
		~CellHistoryHealth() override;
		CellHistoryHealth(const CellHistoryHealth&) = delete;
		CellHistoryHealth& operator=(const CellHistoryHealth&) = delete;

		Dia::Core::StringCRC GetReporterName() const override;
		Dia::Observation::Health::Health Report() const override;

	private:
		const CellHistoryRecorder& mRecorder;
	};

	// Metrics wrapper for recorder: tracks memory usage, sample duration, version gate skips, and health.
	class CellHistoryObservation
	{
	public:
		explicit CellHistoryObservation(CellHistoryRecorder& recorder);
		~CellHistoryObservation();
		CellHistoryObservation(const CellHistoryObservation&) = delete;
		CellHistoryObservation& operator=(const CellHistoryObservation&) = delete;

		// Call after Sample() to update metrics.
		void OnRecorderUpdate();

	private:
		CellHistoryRecorder& mRecorder;
		CellHistoryHealth mHealth;

		// Metric handles (non-owning; registered with global MetricRegistry).
		Dia::Observation::Metric::Gauge* mMetricCellBytes = nullptr;
		Dia::Observation::Metric::Gauge* mMetricAggregateBytes = nullptr;
		Dia::Observation::Metric::Gauge* mMetricEventBytes = nullptr;
		Dia::Observation::Metric::Gauge* mMetricWindowLength = nullptr;
		Dia::Observation::Metric::Counter* mMetricSamples = nullptr;
		Dia::Observation::Metric::Counter* mMetricEvents = nullptr;
		Dia::Observation::Metric::Gauge* mMetricSampleMs = nullptr;
		Dia::Observation::Metric::Counter* mMetricVersionGateSkips = nullptr;

		uint32_t mPreviousSampleCount = 0;
		uint32_t mPreviousEventCount = 0;
		uint64_t mPreviousVersionGateSkips = 0;
	};
}
