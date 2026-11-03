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
#include <DiaCore/Threading/Mutex.h>
#include <DiaObservation/Log/LogLevel.h>

#include "DiaDebugServer/CommandDispatcher.h"
#include "DiaDebugServer/QueryRegistry.h"
#include "DiaDebugServer/DebugServerLogSink.h"
#include "DiaDebugServer/IDebugStateProvider.h"
#include "DiaDebugServer/ObservationBridge.h"
#include "DiaDebugServer/ISimHostedCatalogue.h"
#include "DiaDebugServer/ISimRequestSink.h"
#include "DiaDebugServer/SimHelpInfo.h"
#include "DiaDebugServer/SimHostedTypes.h"
#include "DiaDebugServer/SimPendingTable.h"

#include <cstdint>
#include <functional>
#include <memory>

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
		struct TopicStats
		{
			Dia::Core::StringCRC topic;
			int messagesSent;
			int messagesDropped;
			float dropRatePercent;

			TopicStats() : messagesSent(0), messagesDropped(0), dropRatePercent(0.0f) {}
		};

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

			uint64_t lastDropNotifyTimestampUs; // microsecond timestamp of last editor.notification send

			static constexpr int kMaxTrackedTopics = 32;
			TopicStats topicStats[kMaxTrackedTopics];
			int topicStatCount;

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
				, lastDropNotifyTimestampUs(0)
				, topicStatCount(0)
			{}
		};

		struct DebugServerTestAccess;

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
		// The merged command list served by the "list_commands" query: DiaAPI commands plus the sim-hosted catalogue
		// ('sim' may be null).  Result:
		//   "api_commands":     every name once (DiaAPI first, then sim-hosted)
		//   "command_details":  one object per name: name, source ("api"|"sim"), kind (sim only: "query"|"command"),
		//                       description, category, owner, noArgs, params[] (name,type,required,description,unit,
		//                       hasRange,min,max,enum)
		// A name present in both is reported once, from the DiaAPI (Main-hosted) entry, with a Warning.
		Json::Value BuildMergedCommandList(const ISimHostedCatalogue* sim);

		class DebugServer : public ISimHostedCatalogue
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
			// Main-thread only.
			void NotifySubscribers(const Dia::Core::StringCRC& dataType, const Json::Value& payload);

			// Thread-safe publish of a "virtual topic" value.  Callable from ANY thread.
			// The newest payload per topic is retained; Tick (Main) delivers it to
			// current subscribers (latest wins if published several times between
			// ticks), and a client that subscribes later receives the retained value
			// right after its subscribe ack.  No replay if nothing was published.
			void PublishTopic(const Dia::Core::StringCRC& topic, const Json::Value& payload);

			// Thread-safe, ORDERED, lossless-up-to-capacity publish.  Callable from ANY thread (e.g. SimPU
			// inspector sources).  Every call is queued in FIFO order and delivered to the topic's current
			// subscribers by Tick (Main), the same fan-out as NotifySubscribers.  Unlike PublishTopic nothing
			// is collapsed and nothing is retained or replayed to late subscribers.  When the queue already
			// holds kOrderedQueueCapacity undrained events the NEW event is dropped (order of what is kept
			// never changes), counted in GetOrderedDroppedCount() and logged.
			static const unsigned int kOrderedQueueCapacity = 256;
			void PublishTopicOrdered(const Dia::Core::StringCRC& topic, const Json::Value& payload);

			// Total events dropped by PublishTopicOrdered because the queue was full.  Callable from ANY thread.
			unsigned int GetOrderedDroppedCount() const;

			// Total (connection, topic) subscriptions across all topics.  Callable from ANY thread; use this
			// instead of GetStats().subscriptionCount, which Main writes unsynchronised.
			unsigned int GetSubscriptionCount() const;

			// Number of distinct connections currently subscribed to `topic` (0 if none/unknown).  Callable from ANY thread.
			unsigned int GetTopicSubscriberCount(const Dia::Core::StringCRC& topic) const;

			// --- Sim-hosted queries (Main thread only) --------------------------
			// A sim-hosted name is not executed on Main: HandleCommand posts a SimRequestEvent
			// through the sink and defers the COMMAND_RESPONSE until CompleteSimReply (called
			// on Main when the reply event is drained) or a 2000 ms timeout (swept in Tick).
			// Dispatch order: sim-hosted, QueryRegistry, DiaAPI fallback.
			// Sink must outlive the DebugServer or be cleared with nullptr.
			// Logs Info on attach (non-null) and detach (null).
			void SetSimRequestSink(ISimRequestSink* sink);

			// Registers a sim-hosted name together with its help (copied into the catalogue; 'info' need not outlive the call).
			// Re-registering a name with the SAME kind replaces its entry (help included).  False (Error log) if the name is
			// already sim-hosted with a DIFFERENT kind, the name exists in QueryRegistry, or the 32-name table is full.
			// The reverse is also refused: QueryRegistry::Register of an already sim-hosted name
			// logs an Error and does not register (veto installed by the DebugServer constructor).
			bool RegisterSimHosted(Dia::Core::StringCRC name, SimKind kind, const SimHelpStored& info);
			// Removes the entry and its help.  Unknown names are ignored.
			void UnregisterSimHosted(Dia::Core::StringCRC name);

			// Atomically replaces the whole sim-hosted table AND catalogue with 'names' (announcement resync install).
			// Each entry carries a pointer to its help (required; copied).  Duplicate names in the input: the LAST entry
			// wins (kind and help).  Returns false and leaves table and catalogue untouched (Error log) if a name is
			// null/empty, a help pointer is null, the deduplicated set exceeds the table capacity (32), or a name already
			// exists in the QueryRegistry.  count==0 clears both.
			bool ReplaceSimHosted(const SimHostedName* names, uint32_t count);

			// Entry count and wrapping uint32 sum of SimNameSetEntryHash over every sim-hosted entry
			// (order independent; name+kind only, help is not hashed).  Empty table gives 0 / 0.
			void GetSimHostedDigest(uint32_t& count, uint32_t& hash) const;

			// Read-only catalogue access (Main thread only).  Entries are in registration order.
			// (ISimHostedCatalogue: pass a pointer to this to the consumers of command help.)
			uint32_t SimHostedCount() const override { return mSimNames.Size(); }
			// False if index >= SimHostedCount().  'out.help' stays valid until the next catalogue mutation.
			bool GetSimHostedEntry(uint32_t index, SimHostedEntryView& out) const override;
			// Null if 'name' is not sim-hosted.
			const SimHelpStored* FindSimHostedHelp(Dia::Core::StringCRC name) const override;

			// Sends the deferred COMMAND_RESPONSE for reply.requestId.  Late/unknown ids are dropped.
			void CompleteSimReply(const SimReplyEvent& reply);

			// Clock hook (milliseconds, monotonic) used for sim timeouts; null restores the default.
			typedef uint64_t (*NowMsFunction)();
			void SetNowMsFunction(NowMsFunction fn);

			// Outgoing-message hook: if set, every message that would go to a connection is
			// handed to it instead of the WebSocket.  Used by tests (no socket required).
			typedef void (*SendHook)(void* context, int connId, const dia::debug::DebugMessage& msg);
			void SetSendHook(SendHook hook, void* context) { mSendHook = hook; mSendHookContext = context; }

		private:
			friend struct DebugServerTestAccess;

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

			static bool SimNameVeto(void* context, const Dia::Core::StringCRC& name);	// QueryRegistry register veto
			void SweepSimTimeouts();
			void SendSimError(int connId, const char* commandName, const char* error);
			uint64_t NowMs() const;

			void SendGameInfo(int connId);
			const char* GetCurrentStageName() const;
			int  GetProcessingUnitCount() const;

			void RegisterProtocolCommands();
			void SendProtoMessage(int connId, const dia::debug::DebugMessage& msg);
			void BroadcastProtoMessage(const dia::debug::DebugMessage& msg);
			void SendJsonToConnection(int connId, const Json::Value& json);
			void BroadcastJson(const Json::Value& json);
			// Main-thread-only senders shared by NotifySubscribers / PublishTopic drain / replay.
			void NotifySubscribersMainThread(const Dia::Core::StringCRC& dataType, const Json::Value& payload);
			void SendDataUpdateToConnection(int connId, const Dia::Core::StringCRC& topic, const Json::Value& payload);
			void DrainPublishedTopics();
			void DrainOrderedTopics();
			void RebuildSubscriberCounts();	// Main only; recomputes mSubscriberCounts from mClientTaps after any mutation
			void RecordTopicSent(const Dia::Core::StringCRC& topic, bool success);
			void SendDropNotification(int connId);

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

			// PublishTopic state (any-thread writers, Main drain).  Guarded by mPublishMutex.
			struct PublishedTopic {
				Dia::Core::StringCRC topic;
				Json::Value          latest;
				bool                 pending;
			};
			static const unsigned int kMaxPublishedTopics = 32;
			Dia::Core::Mutex mPublishMutex;
			Dia::Core::Containers::DynamicArrayC<PublishedTopic, kMaxPublishedTopics> mPublished;

			// PublishTopicOrdered state: fixed ring FIFO (any-thread writers, Main drain).  Guarded by mOrderedMutex.
			struct OrderedEvent {
				Dia::Core::StringCRC topic;
				Json::Value          payload;
			};
			mutable Dia::Core::Mutex mOrderedMutex;
			OrderedEvent             mOrdered[kOrderedQueueCapacity];
			unsigned int             mOrderedHead;
			unsigned int             mOrderedCount;
			unsigned int             mOrderedDropped;

			// Per-topic distinct-connection subscriber counts.  Written on Main (RebuildSubscriberCounts), read from any thread.
			struct TopicSubscribers {
				Dia::Core::StringCRC topic;
				unsigned int         count;
			};
			mutable Dia::Core::Mutex mSubscriberMutex;
			Dia::Core::Containers::DynamicArrayC<TopicSubscribers, 64> mSubscriberCounts;

			// Sim-hosted state (Main thread only; no locking).
			struct SimName {
				Dia::Core::StringCRC name;
				SimKind              kind;
			};
			static const unsigned int kMaxSimNames = 32;
			ISimRequestSink*   mSimSink;
			SimPendingTable    mSimPending;
			Dia::Core::Containers::DynamicArrayC<SimName, kMaxSimNames> mSimNames;
			// Help catalogue, parallel to mSimNames (index i is the help of mSimNames[i]).  kMaxSimNames x ~7 KB lives on
			// the heap, allocated once in the constructor, so sizeof(DebugServer) stays small.
			std::unique_ptr<SimHelpStored[]> mSimHelp;
			NowMsFunction      mNowMsFn;
			SendHook           mSendHook;
			void*              mSendHookContext;
			bool               mLoggedLateSimReply;

			uint64_t mStartTimestamp;
			bool     mStarted;
		};
	}
}

#endif // DIA_DEBUG_SERVER_H
