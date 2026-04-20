#ifndef DIA_DEBUG_SERVER_MODULE_H
#define DIA_DEBUG_SERVER_MODULE_H

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>

#include "DiaDebugServer/SubscriptionManager.h"
#include "DiaDebugServer/CommandDispatcher.h"
#include "DiaDebugServer/StateSerializer.h"

#include <cstdint>

namespace Dia
{
	namespace WebSocket
	{
		class Server;
		struct Message;
	}

	namespace DebugServer
	{
		struct ServerStats
		{
			float debugServerOverheadMs;
			float serializationTimeMs;
			float broadcastTimeMs;
			int subscriptionCount;

			float bytesSentPerSec;
			int messageQueueSize;
			int messagesDropped;
			int averageMessageSizeBytes;

			int connectionCount;
			int messagesSentTotal;
			int messagesReceivedTotal;
			int uptimeSeconds;

			ServerStats()
				: debugServerOverheadMs(0.0f)
				, serializationTimeMs(0.0f)
				, broadcastTimeMs(0.0f)
				, subscriptionCount(0)
				, bytesSentPerSec(0.0f)
				, messageQueueSize(0)
				, messagesDropped(0)
				, averageMessageSizeBytes(0)
				, connectionCount(0)
				, messagesSentTotal(0)
				, messagesReceivedTotal(0)
				, uptimeSeconds(0)
			{}
		};

		class DebugServerModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			DebugServerModule(Dia::Application::ProcessingUnit* pu);
			virtual ~DebugServerModule();

			bool StartServer();
			void StopServer();
			bool IsServerRunning() const;

			void SetPort(uint16_t port) { mPort = port; }
			void EnableAutoStart(bool enable) { mAutoStart = enable; }

			int GetConnectionCount() const;
			const ServerStats& GetStats() const { return mStats; }

			SubscriptionManager& GetSubscriptionManager() { return mSubscriptionManager; }
			CommandDispatcher& GetCommandDispatcher() { return mCommandDispatcher; }

			virtual const char* GetStateObjectType() const override { return "DebugServerModule"; }

		protected:
			virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override;
			virtual void DoUpdate() override;
			virtual void DoStop() override;

		private:
			void HandleConnection(int connId, bool connected);
			void HandleMessage(int connId, const Dia::WebSocket::Message& msg);

			void HandleSubscribe(int connId, const Json::Value& json);
			void HandleUnsubscribe(int connId, const Json::Value& json);
			void HandleCommand(int connId, const Json::Value& json);
			void HandleHandshake(int connId, const Json::Value& json);

			void BroadcastCoreMetrics();
			void NotifySubscribers(const Dia::Core::StringCRC& dataType, const Json::Value& payload);

			void RegisterProtocolCommands();
			void SendJsonToConnection(int connId, const Json::Value& json);
			void BroadcastJson(const Json::Value& json);

			Dia::WebSocket::Server* mServer;
			uint16_t mPort;
			bool mAutoStart;

			float mMetricsBroadcastInterval;
			float mMetricsTimer;

			ServerStats mStats;
			SubscriptionManager mSubscriptionManager;
			CommandDispatcher mCommandDispatcher;

			uint64_t mStartTimestamp;
		};
	}
}

#endif // DIA_DEBUG_SERVER_MODULE_H
