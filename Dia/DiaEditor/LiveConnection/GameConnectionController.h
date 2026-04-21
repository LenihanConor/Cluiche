#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class GameConnectionManager;

		// Drives the Game Connection panel: owns the editor-side UI state
		// machine (disconnected / connecting / connected), registers the
		// game_connection.* WebUIBridge handlers, publishes topic pushes,
		// and persists the last-used URL.
		//
		// Slice 1 is stub-only: Connect() fakes a successful handshake and
		// emits a canned game_info payload instead of opening a socket. The
		// real GameConnectionManager path is left in place for Slice 2.
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

			void Initialize(WebUIBridge* bridge, GameConnectionManager* manager);
			void Shutdown();

			// Pumped by the owning module each frame.
			void Update(float deltaTime);

			void SetPersistencePath(const char* path);
			void LoadPersistedUrl();
			void SavePersistedUrl() const;

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

			// Stub/real dispatch (Slice 1 always uses stub).
			void BeginConnectStub();
			void CompleteConnectStub();
			void BeginConnectReal();
			void DisconnectInternal(const char* reason);

			// Request handlers (invoked via WebUIBridge).
			Json::Value HandleConnectRequest(const Json::Value& data);
			Json::Value HandleDisconnectRequest(const Json::Value& data);
			Json::Value HandleGetStateRequest(const Json::Value& data);

			void BuildStatePayload(Json::Value& out) const;
			void SetLastError(const char* msg);
			void SetUrl(const char* url);

			WebUIBridge* mBridge;
			GameConnectionManager* mManager;

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

			static const float kStubConnectDelaySeconds;
			static const float kHeartbeatIntervalSeconds;
		};
	}
}
