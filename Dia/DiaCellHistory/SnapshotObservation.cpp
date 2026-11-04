#include <DiaCellHistory/SnapshotObservation.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>

#include <chrono>

namespace Dia::CellHistory
{
	namespace
	{
		const char* const kMetricSnapshots = "dia.cellhistory.snapshots";
		const char* const kMetricTakeMs = "dia.cellhistory.snapshot_take_ms";
		const char* const kMetricRestoreMs = "dia.cellhistory.snapshot_restore_ms";
		const char* const kMetricBytes = "dia.cellhistory.snapshot_bytes";
		const char* const kMetricFailures = "dia.cellhistory.snapshot_failures";
		const char* const kMetricQueries = "dia.cellhistory.snapshot_queries_served";
	}

	SnapshotObservation::SnapshotObservation(ClockMsFn clock, void* clockUser)
		: mClock(clock), mClockUser(clockUser)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		using Dia::Core::StringCRC;
		mSnapshots = metrics.RegisterGauge(StringCRC(kMetricSnapshots));
		mTakeMs = metrics.RegisterGauge(StringCRC(kMetricTakeMs));
		mRestoreMs = metrics.RegisterGauge(StringCRC(kMetricRestoreMs));
		mBytes = metrics.RegisterGauge(StringCRC(kMetricBytes));
		mFailures = metrics.RegisterCounter(StringCRC(kMetricFailures));
		mQueries = metrics.RegisterCounter(StringCRC(kMetricQueries));
		if (mSnapshots) { mSnapshots->Set(0.0); }
		if (mTakeMs) { mTakeMs->Set(0.0); }
		if (mRestoreMs) { mRestoreMs->Set(0.0); }
		if (mBytes) { mBytes->Set(0.0); }
	}

	double SnapshotObservation::NowMs() const
	{
		if (mClock != nullptr) { return mClock(mClockUser); }
		using namespace std::chrono;
		return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
	}

	void SnapshotObservation::SetSnapshotCount(uint32_t readyCount)
	{
		if (mSnapshots) { mSnapshots->Set(double(readyCount)); }
	}

	void SnapshotObservation::OnTakeCompleted(double captureMs, double writeMs, uint64_t bytes)
	{
		if (mTakeMs) { mTakeMs->Set(captureMs + writeMs); }
		if (mBytes) { mBytes->Set(double(bytes)); }
	}

	void SnapshotObservation::OnRestoreCompleted(double elapsedMs)
	{
		if (mRestoreMs) { mRestoreMs->Set(elapsedMs); }
	}

	void SnapshotObservation::OnFailure() { if (mFailures) { mFailures->Inc(); } }
	void SnapshotObservation::OnQueryServed() { if (mQueries) { mQueries->Inc(); } }
}
