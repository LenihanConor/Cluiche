#pragma once

// DebugServerTopicSink: production ITopicSink forwarding to DebugServer::PublishTopic (thread-safe from any PU; the server
// retains the latest value per topic and replays it to late subscribers). The server is borrowed and must outlive the sink.
// Wiring it into a module (resolving the "DebugServerService" stream) is host wiring, not done here.

#include <DiaCellLayers/ITopicSink.h>

namespace Dia::DebugServer { class DebugServer; }

namespace Dia::CellLayers
{
	class DebugServerTopicSink : public ITopicSink
	{
	public:
		explicit DebugServerTopicSink(Dia::DebugServer::DebugServer& server) : mServer(server) {}
		void Publish(const Dia::Core::StringCRC& topic, const Json::Value& payload) override;
		unsigned int SubscriberCount(const Dia::Core::StringCRC& topic) const override;

	private:
		Dia::DebugServer::DebugServer&	mServer;
	};
}
