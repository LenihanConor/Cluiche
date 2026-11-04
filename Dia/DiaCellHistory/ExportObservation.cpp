#include <DiaCellHistory/ExportObservation.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>

#include <chrono>

namespace Dia::CellHistory
{
	ExportObservation::ExportObservation(ClockMsFn clock, void* clockUser)
		: mClock(clock), mClockUser(clockUser)
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		using Dia::Core::StringCRC;
		mExports = metrics.RegisterCounter(StringCRC("dia.cellhistory.exports"));
		mFailures = metrics.RegisterCounter(StringCRC("dia.cellhistory.export_failures"));
		mMs = metrics.RegisterGauge(StringCRC("dia.cellhistory.export_ms"));
		mBytes = metrics.RegisterGauge(StringCRC("dia.cellhistory.export_bytes"));
		if (mMs) { mMs->Set(0.0); }
		if (mBytes) { mBytes->Set(0.0); }
	}

	double ExportObservation::NowMs() const
	{
		if (mClock != nullptr) { return mClock(mClockUser); }
		using namespace std::chrono;
		return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
	}

	void ExportObservation::OnFailed()
	{
		if (mFailures) { mFailures->Inc(); }
	}

	void ExportObservation::OnExported(double elapsedMs, uint64_t bytes)
	{
		if (mExports) { mExports->Inc(); }
		if (mMs) { mMs->Set(elapsedMs); }
		if (mBytes) { mBytes->Set(double(bytes)); }
	}
}
