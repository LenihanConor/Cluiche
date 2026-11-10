#pragma once

// IViewHistorySource: optional hook through which a history system supplies the "history" value of the cells.view.state
// topic (e.g. DiaCellHistory reporting live vs a scrubbed sample). Installed on ViewStatePublisher (borrowed, not owned);
// with no source installed the topic carries history: null. Generic: no game types (CL-005), no singletons (CL-006).
//
// HistoryField() returns the JSON published under "history". HistoryStamp() must change whenever HistoryField() would
// return a different value; the publisher compares stamps each frame to decide whether to republish, and only calls
// HistoryField() when it actually publishes. Same-PU contract as the publisher (SimPU).

#include <json/json.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	class IViewHistorySource
	{
	public:
		virtual ~IViewHistorySource() {}

		virtual Json::Value	HistoryField() const = 0;
		virtual uint64_t	HistoryStamp() const = 0;
	};
}
