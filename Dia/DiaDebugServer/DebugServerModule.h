#ifndef DIA_DEBUG_SERVER_MODULE_H
#define DIA_DEBUG_SERVER_MODULE_H

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/CRC/StringCRC.h>

#include "DiaDebugServer/SubscriptionManager.h"
#include "DiaDebugServer/CommandDispatcher.h"
#include "DiaDebugServer/QueryRegistry.h"
#include "DiaDebugServer/StateSerializer.h"
#include "DiaDebugServer/DebugServerLogSink.h"
#include <DiaLogger/LogLevel.h>

#include <cstdint>

namespace dia { namespace debug { class DebugMessage; } }

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
			static const Dia::Core::StringCRC& kTypeId;

			DebugServerModule(Dia::Application::ProcessingUnit* pu);
			virtual ~DebugServerModule();

			bool StartServer();
			void StopServer();
			bool IsServerRunning() const;

			void SetPort(uint16_t port) { mPort = port; }
			void EnableAutoStart(bool enable) { mAutoStart = enable; }

			// Sets the game identity published to newly connected clients as a game_info payload.
			// processingUnitCountProvider lets the server refresh the count at publish time.
			void SetGameInfo(const char* name, const char* build);
			void SetLogSinkLevel(Dia::Logger::LogLevel level) { mLogSink.SetLevelThreshold(level); }

			int GetConnectionCount() const;
			const ServerStats& GetStats() const { return mStats; }

			SubscriptionManager& GetSubscriptionManager() { return mSubscriptionManager; }
			CommandDispatcher& GetCommandDispatcher() { return mCommandDispatcher; }
			QueryRegistry& GetQueryRegistry() { return mQueryRegistry; }

			// Send a data-update to all subscribers of the given topic.
			// Uses MESSAGE_TYPE_DATA_UPDATE envelope; payload is arbitrary JSON.
			void NotifySubscribers(const Dia::Core::StringCRC& dataType, const Json::Value& payload);

			virtual const char* GetStateObjectType() const override { return "DebugServerModule"; }

		protected:
			virtual StateObject::OpertionResponse DoStart(const IStartData* startData) override;
			virtual void DoUpdate() override;
			virtual void DoStop() override;

		private:
			void HandleConnection(int connId, bool connected);
			void HandleMessage(int connId, const Dia::WebSocket::Message& msg);

			void HandleSubscribe(int connId, const dia::debug::DebugMessage& msg);
			void HandleUnsubscribe(int connId, const dia::debug::DebugMessage& msg);
			void HandleCommand(int connId, const dia::debug::DebugMessage& msg);
			void HandleHandshake(int connId, const dia::debug::DebugMessage& msg);
			void HandlePing(int connId, const dia::debug::DebugMessage& msg);

			void SendGameInfo(int connId);
			const char* GetCurrentPhaseName() const;
			int GetProcessingUnitCount() const;

			void BroadcastCoreMetrics();

			void RegisterProtocolCommands();
			void SendProtoMessage(int connId, const dia::debug::DebugMessage& msg);
			void BroadcastProtoMessage(const dia::debug::DebugMessage& msg);
			void SendJsonToConnection(int connId, const Json::Value& json);
			void BroadcastJson(const Json::Value& json);

			Dia::WebSocket::Server* mServer;
			uint16_t mPort;
			bool mAutoStart;

			static const unsigned int kMaxGameFieldLength = 128;
			char mGameName[kMaxGameFieldLength];
			char mGameBuild[kMaxGameFieldLength];

			float mMetricsBroadcastInterval;
			float mMetricsTimer;

			ServerStats mStats;
			SubscriptionManager mSubscriptionManager;
			CommandDispatcher mCommandDispatcher;
			QueryRegistry mQueryRegistry;
			DebugServerLogSink mLogSink;

			uint64_t mStartTimestamp;
		};
	}
}

#endif // DIA_DEBUG_SERVER_MODULE_H
