#ifndef DIA_DEBUG_SERVER_H
#define DIA_DEBUG_SERVER_H

////////////////////////////////////////////////////////////////////////////////
// Filename: DebugServer.h
//
// Pure library capability — owns the WebSocket server, subscription manager,
// query registry, command dispatcher, and log sink.  Does NOT inherit from
// any application framework lifecycle type.  A host (e.g. a v2 Module
// adapter in CluicheGameBaseline) drives Start/Tick/Stop and supplies an
// IDebugStateProvider for introspection queries.
////////////////////////////////////////////////////////////////////////////////

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaObservation/Log/LogLevel.h>

#include "DiaDebugServer/CommandDispatcher.h"
#include "DiaDebugServer/QueryRegistry.h"
#include "DiaDebugServer/DebugServerLogSink.h"
#include "DiaDebugServer/IDebugStateProvider.h"
#include "DiaDebugServer/ObservationBridge.h"

#include <cstdint>
#include <functional>

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
			int   subscriptionCount;

			float bytesSentPerSec;
			int   messageQueueSize;
			int   messagesDropped;
			int   averageMessageSizeBytes;

			int   connectionCount;
			int   messagesSentTotal;
			int   messagesReceivedTotal;
			int   uptimeSeconds;

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

		// Pure WebSocket-based debug server.  Not a Module.
		//
		// Lifecycle (driven by the host):
		//   DebugServer server;
		//   server.Configure(port, autoStart);          // optional, before Start
		//   server.SetStateProvider(&myProvider);       // before Start
		//   server.SetGameInfo("MyGame", "1.2.3");      // optional
		//   server.Start();
		//   ... every tick:  server.Tick(deltaTimeSec);
		//   server.Stop();
		//
		// Thread safety: not thread-safe; all methods must be called from the
		// host's tick thread (typically the main thread / main PU).
		class DebugServer
		{
		public:
			DebugServer();
			~DebugServer();

			DebugServer(const DebugServer&) = delete;
			DebugServer& operator=(const DebugServer&) = delete;

			// --- Configuration --------------------------------------------------

			void SetPort(uint16_t port)             { mPort = port; }
			void EnableAutoStart(bool enable)       { mAutoStart = enable; }
			void SetGameInfo(const char* name, const char* build);
			void SetDiagamePath(const char* diagamePath);
			void SetLogSinkLevel(Dia::Observation::Log::LogLevel level) { mLogSink.SetLevelThreshold(level); }

			// State provider — the host translates its lifecycle framework's
			// introspection (stage, PUs, modules) into this narrow interface.
			// Pointer must outlive the DebugServer or be cleared with nullptr
			// before destruction.
			void SetStateProvider(IDebugStateProvider* provider) { mStateProvider = provider; }

			// Push-based stage-transition notification.  The host registers a
			// callback that fires whenever the $lifecycle stream emits a
			// kStageTransitionCommitted event.  Called on whatever thread the
			// stream emits from (typically SimPU).  Pass nullptr to clear.
			using StageTransitionCallback = std::function<void(Dia::Core::StringCRC fromStage, Dia::Core::StringCRC toStage)>;
			void SetStageTransitionCallback(StageTransitionCallback cb);

			// Broadcast a stage transition to all subscribed clients.  Intended to
			// be called from the StageTransitionCallback registered above, or
			// directly when the host has transition information available.
			void BroadcastStageTransition(Dia::Core::StringCRC fromStage, Dia::Core::StringCRC toStage);

			// --- Lifecycle ------------------------------------------------------

			// Create the underlying WebSocket server, register handlers, and
			// (if EnableAutoStart(true)) start listening.  Safe to call once.
			void Start();

			// Advance server state for the given wall-clock delta.  Drains the
			// WebSocket queue, flushes the log sink, and polls the state provider
			// for stage changes.
			void Tick(float deltaTime);

			// Stop the WebSocket server and destroy it.  Safe to call once.
			void Stop();

			// --- Observation Bridge ----------------------------------------------
			// Call after Start() once session info is available.
			void StartObservationBridge(const char* sessionId, int64_t epochOffsetNs);

			// --- Manual server control (rarely used; Start/Stop usually suffice)
			bool StartServer();
			void StopServer();
			bool IsServerRunning() const;

			// --- Queries --------------------------------------------------------

			int GetConnectionCount() const;
			const ServerStats& GetStats() const { return mStats; }

			CommandDispatcher&   GetCommandDispatcher()   { return mCommandDispatcher; }
			QueryRegistry&       GetQueryRegistry()       { return mQueryRegistry; }

			// Send a data-update to all subscribers of the given topic.
			void NotifySubscribers(const Dia::Core::StringCRC& dataType, const Json::Value& payload);

		private:
			struct ClientTap {
				int connId;
				Dia::Core::StringCRC streamId;
				unsigned int tapId;
			};

			void HandleConnection(int connId, bool connected);
			void HandleMessage(int connId, const Dia::WebSocket::Message& msg);

			void HandleSubscribe(int connId, const dia::debug::DebugMessage& msg);
			void HandleUnsubscribe(int connId, const dia::debug::DebugMessage& msg);
			void HandleCommand(int connId, const dia::debug::DebugMessage& msg);
			void HandleHandshake(int connId, const dia::debug::DebugMessage& msg);
			void HandlePing(int connId, const dia::debug::DebugMessage& msg);

			void SendGameInfo(int connId);
			const char* GetCurrentStageName() const;
			int  GetProcessingUnitCount() const;

			void RegisterProtocolCommands();
			void SendProtoMessage(int connId, const dia::debug::DebugMessage& msg);
			void BroadcastProtoMessage(const dia::debug::DebugMessage& msg);
			void SendJsonToConnection(int connId, const Json::Value& json);
			void BroadcastJson(const Json::Value& json);

			Dia::WebSocket::Server* mServer;
			IDebugStateProvider*    mStateProvider;

			uint16_t mPort;
			bool     mAutoStart;

			static const unsigned int kMaxGameFieldLength = 128;
			static const unsigned int kMaxPathLength = 512;
			char mGameName[kMaxGameFieldLength];
			char mGameBuild[kMaxGameFieldLength];
			char mDiagamePath[kMaxPathLength];

			ServerStats          mStats;
			Dia::Core::Containers::DynamicArrayC<ClientTap, 64> mClientTaps;
			StageTransitionCallback                             mStageTransitionCallback;
			CommandDispatcher    mCommandDispatcher;
			QueryRegistry        mQueryRegistry;
			DebugServerLogSink   mLogSink;
			ObservationBridge*   mObservationBridge;

			uint64_t mStartTimestamp;
			bool     mStarted;
		};
	}
}

#endif // DIA_DEBUG_SERVER_H
