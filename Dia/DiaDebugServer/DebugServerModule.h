#ifndef DIA_DEBUG_SERVER_MODULE_H
#define DIA_DEBUG_SERVER_MODULE_H

#include <DiaApplicationFlow/Module.h>
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

		// DiaApplicationFlow v2 Module.
		//
		// Observes the running Application via its IApplicationControl handle
		// and polls for stage transitions in DoUpdate — v2 has no MessageBus.
		// Collects frame time from the DoUpdate deltaTime and memory from the
		// OS; v1's separate MetricsCollectorModule is gone.
		class DebugServerModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit DebugServerModule(const Dia::Core::StringCRC& instanceId);
			~DebugServerModule() override;

			bool StartServer();
			void StopServer();
			bool IsServerRunning() const;

			void SetPort(uint16_t port) { mPort = port; }
			void EnableAutoStart(bool enable) { mAutoStart = enable; }

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

		protected:
			// v2 module lifecycle
			void OnConfigure(const char* configJson) override;
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			void HandleConnection(int connId, bool connected);
			void HandleMessage(int connId, const Dia::WebSocket::Message& msg);

			void HandleSubscribe(int connId, const dia::debug::DebugMessage& msg);
			void HandleUnsubscribe(int connId, const dia::debug::DebugMessage& msg);
			void HandleCommand(int connId, const dia::debug::DebugMessage& msg);
			void HandleHandshake(int connId, const dia::debug::DebugMessage& msg);
			void HandlePing(int connId, const dia::debug::DebugMessage& msg);

			void SendGameInfo(int connId);
			const char* GetCurrentStageName() const;
			int GetProcessingUnitCount() const;

			void BroadcastCoreMetrics();
			void BroadcastStageTransition(const Dia::Core::StringCRC& from,
			                               const Dia::Core::StringCRC& to);

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

			// Frame-time smoothing — 30-frame moving average.
			float mFrameTimeAccumMs;
			int   mFrameTimeSampleCount;
			float mLastFpsSample;
			float mLastFrameTimeMsSample;

			// Last-observed stage, for edge detection in DoUpdate.  Initialized
			// in DoStart.
			Dia::Core::StringCRC mLastObservedStage;

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
