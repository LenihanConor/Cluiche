#include <DiaCellHistory/QueryObservation.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>

namespace Dia::CellHistory
{
	QueryObservation::QueryObservation()
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		using Dia::Core::StringCRC;
		mByKind[0] = metrics.RegisterCounter(StringCRC("dia.cellhistory.query_meta"));
		mByKind[1] = metrics.RegisterCounter(StringCRC("dia.cellhistory.query_aggregates"));
		mByKind[2] = metrics.RegisterCounter(StringCRC("dia.cellhistory.query_events"));
		mByKind[3] = metrics.RegisterCounter(StringCRC("dia.cellhistory.query_cells"));
		mBytes = metrics.RegisterCounter(StringCRC("dia.cellhistory.query_bytes"));
		mSlices = metrics.RegisterCounter(StringCRC("dia.cellhistory.query_aggregates_slices"));
		mStrided = metrics.RegisterCounter(StringCRC("dia.cellhistory.query_aggregates_strided"));
		mMaxReply = metrics.RegisterGauge(StringCRC("dia.cellhistory.query_max_reply_bytes"));
		if (mMaxReply) { mMaxReply->Set(0.0); }
	}

	void QueryObservation::OnAggregatesShape(bool sliced, bool strided)
	{
		if (sliced && mSlices) { mSlices->Inc(); }
		if (strided && mStrided) { mStrided->Inc(); }
	}

	void QueryObservation::OnQuery(Kind kind, uint64_t bytes)
	{
		const uint32_t i = uint32_t(kind);
		if (i < 4 && mByKind[i]) { mByKind[i]->Inc(); }
		if (mBytes) { mBytes->Inc(bytes); }
		if (bytes > mMaxReplyBytes)
		{
			mMaxReplyBytes = bytes;
			if (mMaxReply) { mMaxReply->Set(double(bytes)); }
		}
	}
}
