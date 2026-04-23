#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstdint>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class GameConnectionManager;
		class EditorView;

		// Drives the Game Connection panel: owns the editor-side UI state
		// machine (disconnected / connecting / connected), registers the
		// game_connection.* WebUIBridge handlers, publishes topic pushes,
		// and persists the last-used URL.
		//
		// Default transport opens a real WebSocket via GameConnectionManager
		// and speaks the DiaDebugProtocol game_info / ping / pong envelopes.
		// A stub path is kept (mUseStub) for tests that do not want a socket.
		class GameConnectionController
		{
		public:
			enum class State
			{
				kDisconnected,
				kConnecting,
				kConnected
			};

			GameConnectionController();
			~GameConnectionController();

			void Initialize(WebUIBridge* bridge, GameConnectionManager* manager, EditorView* editorView = nullptr);
			void Shutdown();

			// Pumped by the owning module each frame.
			void Update(float deltaTime);

			void SetPersistencePath(const char* path);
			void LoadPersistedUrl();
			void SavePersistedUrl() const;
			void AutoConnect(const char* url);

			// Exposed for tests and direct callers.
			State GetState() const { return mState; }
			const char* GetUrl() const { return mUrl; }
			const char* GetLastError() const { return mLastError; }

		private:
			void RegisterHandlers();

			// State transitions.
			void TransitionTo(State newState);
			void PublishState();
			void PublishHeartbeat();

			// Stub/real dispatch. The real path drives GameConnectionManager;
			// stub mode is kept for tests that do not want to open a socket.
			void BeginConnectStub();
			void CompleteConnectStub();
			void BeginConnectReal();
			void DisconnectInternal(const char* reason);

			// Real-transport callbacks wired through GameConnectionManager.
			void OnManagerConnection(bool connected);
			void OnManagerRawMessage(const char* rawText, const Json::Value& envelope);

			// Parse ws://host:port[/...] into host+port. Returns false on error.
			static bool ParseWebSocketUrl(const char* url, char* outHost, unsigned int hostLen, int& outPort);

			// Request handlers (invoked via WebUIBridge).
			Json::Value HandleConnectRequest(const Json::Value& data);
			Json::Value HandleDisconnectRequest(const Json::Value& data);
			Json::Value HandleGetStateRequest(const Json::Value& data);

			void BuildStatePayload(Json::Value& out) const;
			void SetLastError(const char* msg);
			void SetUrl(const char* url);

			void PushGameConsoleEntry(const char* level, const char* message);

			WebUIBridge* mBridge;
			GameConnectionManager* mManager;
			EditorView* mEditorView;

			State mState;

			static const unsigned int kMaxUrlLength = 256;
			char mUrl[kMaxUrlLength];

			static const unsigned int kMaxErrorLength = 256;
			char mLastError[kMaxErrorLength];

			static const unsigned int kMaxPathLength = 260;
			char mPersistencePath[kMaxPathLength];

			// Stub mode machinery.
			bool mUseStub;
			bool mConnectingPending;
			float mConnectingElapsed;
			float mHeartbeatElapsed;
			float mHeartbeatLastPongMs;
			Json::Value mStubGameInfo;

			// Real transport bookkeeping.
			Json::Value mGameInfo;
			bool mGameInfoReceived;
			float mHandshakeElapsed;
			uint64_t mLastPingSentTs;
			uint64_t mLastPongReceivedTs;
			float mSinceLastPong;

			static const float kStubConnectDelaySeconds;
			static const float kHeartbeatIntervalSeconds;
			static const float kHandshakeTimeoutSeconds;
			static const float kPongTimeoutSeconds;
		};
	}
}
