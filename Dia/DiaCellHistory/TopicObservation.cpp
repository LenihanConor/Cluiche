#include <DiaCellHistory/TopicObservation.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>

namespace Dia::CellHistory
{
	TopicObservation::TopicObservation()
	{
		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		using Dia::Core::StringCRC;
		mMessages = metrics.RegisterCounter(StringCRC("dia.cellhistory.topic_messages"));
		mBytes = metrics.RegisterCounter(StringCRC("dia.cellhistory.topic_bytes"));
		mDrops = metrics.RegisterCounter(StringCRC("dia.cellhistory.topic_drops"));
	}

	void TopicObservation::OnMessage(uint64_t bytes)
	{
		if (mMessages) { mMessages->Inc(); }
		if (mBytes) { mBytes->Inc(bytes); }
	}

	void TopicObservation::OnDrop()
	{
		if (mDrops) { mDrops->Inc(); }
	}
}
