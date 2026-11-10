#pragma once

// ITopicSink / IMonotonicClock: the two seams ViewStatePublisher publishes through, so tests need neither a DebugServer nor
// a real clock. Generic: no game types (CL-005), no singletons (CL-006). No STL in the API (PD-004).

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <stdint.h>

namespace Dia::CellLayers
{
	// Receives a topic payload. Production adapter: DebugServerTopicSink (forwards to DebugServer::PublishTopic).
	class ITopicSink
	{
	public:
		virtual ~ITopicSink() {}
		virtual void Publish(const Dia::Core::StringCRC& topic, const Json::Value& payload) = 0;
		// Current subscriber count for `topic` (any thread). Default 0 for sinks that do not track subscribers.
		virtual unsigned int SubscriberCount(const Dia::Core::StringCRC& /*topic*/) const { return 0; }
	};

	// Monotonic milliseconds (arbitrary epoch).
	class IMonotonicClock
	{
	public:
		virtual ~IMonotonicClock() {}
		virtual uint64_t NowMs() const = 0;
	};
}
