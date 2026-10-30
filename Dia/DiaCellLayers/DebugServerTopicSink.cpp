#include <DiaCellLayers/DebugServerTopicSink.h>
#include <DiaDebugServer/DebugServer.h>

namespace Dia::CellLayers
{
	void DebugServerTopicSink::Publish(const Dia::Core::StringCRC& topic, const Json::Value& payload)
	{
		mServer.PublishTopic(topic, payload);
	}
}
