#pragma once

// TopicObservation: metrics for the Feature 5 live topics (HistoryTopics). Registered in the constructor with the global
// MetricRegistry (idempotent per name; no unregister, like QueryObservation).
//
// Metrics (counters):
//   - dia.cellhistory.topic_messages: messages accepted by the publisher (both topics)
//   - dia.cellhistory.topic_bytes: serialised bytes of those messages
//   - dia.cellhistory.topic_drops: messages the publisher rejected (queue full)
//
// Same-PU contract (SimPU); no locks.

#include <stdint.h>

namespace Dia::Observation::Metric { class Counter; }

namespace Dia::CellHistory
{
	class TopicObservation
	{
	public:
		TopicObservation();
		TopicObservation(const TopicObservation&) = delete;
		TopicObservation& operator=(const TopicObservation&) = delete;

		void OnMessage(uint64_t bytes);
		void OnDrop();

	private:
		Dia::Observation::Metric::Counter*	mMessages = nullptr;
		Dia::Observation::Metric::Counter*	mBytes = nullptr;
		Dia::Observation::Metric::Counter*	mDrops = nullptr;
	};
}
