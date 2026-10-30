#pragma once

// Test helpers for the editor mirror (Testing/ convention): settable monotonic clock and a topic recorder.

#include <DiaCellLayers/ITopicSink.h>

#include <vector>

namespace Dia::CellLayers::Testing
{
	class FakeClock : public IMonotonicClock
	{
	public:
		uint64_t	nowMs = 0;
		uint64_t	NowMs() const override { return nowMs; }
		void		Advance(uint64_t ms) { nowMs += ms; }
	};

	class TopicRecorder : public ITopicSink
	{
	public:
		struct Entry { Dia::Core::StringCRC topic; Json::Value payload; };
		std::vector<Entry>	entries;

		void Publish(const Dia::Core::StringCRC& topic, const Json::Value& payload) override { entries.push_back({ topic, payload }); }
		size_t				Count() const { return entries.size(); }
		const Json::Value&	Last() const { return entries.back().payload; }
	};
}
