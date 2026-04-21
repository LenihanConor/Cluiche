#include "DiaEditor/LiveConnection/GameConnectionController.h"

#include "DiaEditor/LiveConnection/GameConnectionManager.h"
#include "DiaEditor/UI/WebUIBridge.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <fstream>
#include <sstream>

namespace Dia
{
	namespace Editor
	{
		const float GameConnectionController::kStubConnectDelaySeconds = 0.35f;
		const float GameConnectionController::kHeartbeatIntervalSeconds = 10.0f;

		static const Dia::Core::StringCRC kReqConnect("game_connection.connect");
		static const Dia::Core::StringCRC kReqDisconnect("game_connection.disconnect");
		static const Dia::Core::StringCRC kReqGetState("game_connection.get_state");

		static const char* kTopicState = "game_connection";
		static const char* kTopicHeartbeat = "game_connection_heartbeat";

		static const char* kStateToString(GameConnectionController::State state)
		{
			switch (state)
			{
			case GameConnectionController::State::kDisconnected: return "disconnected";
			case GameConnectionController::State::kConnecting:   return "connecting";
			case GameConnectionController::State::kConnected:    return "connected";
			}
			return "disconnected";
		}

		GameConnectionController::GameConnectionController()
			: mBridge(nullptr)
			, mManager(nullptr)
			, mState(State::kDisconnected)
			, mUseStub(true)
			, mConnectingPending(false)
			, mConnectingElapsed(0.0f)
			, mHeartbeatElapsed(0.0f)
			, mHeartbeatLastPongMs(0.0f)
		{
			mUrl[0] = '\0';
			mLastError[0] = '\0';
			mPersistencePath[0] = '\0';

			mStubGameInfo["name"] = "CluicheEditor Stub Game";
			mStubGameInfo["build"] = "stub-0.1";
			mStubGameInfo["processingUnitCount"] = 1;
			mStubGameInfo["currentPhase"] = "Running";
		}

		GameConnectionController::~GameConnectionController()
		{
			Shutdown();
		}

		void GameConnectionController::Initialize(WebUIBridge* bridge, GameConnectionManager* manager)
		{
			mBridge = bridge;
			mManager = manager;

			RegisterHandlers();
		}

		void GameConnectionController::Shutdown()
		{
			mBridge = nullptr;
			mManager = nullptr;
		}

		void GameConnectionController::Update(float deltaTime)
		{
			if (mState == State::kConnecting && mConnectingPending && mUseStub)
			{
				mConnectingElapsed += deltaTime;
				if (mConnectingElapsed >= kStubConnectDelaySeconds)
				{
					mConnectingPending = false;
					CompleteConnectStub();
				}
			}
			else if (mState == State::kConnected)
			{
				mHeartbeatElapsed += deltaTime;
				if (mHeartbeatElapsed >= kHeartbeatIntervalSeconds)
				{
					mHeartbeatElapsed = 0.0f;
					mHeartbeatLastPongMs = 0.0f;
					PublishHeartbeat();
				}
			}
		}

		void GameConnectionController::SetPersistencePath(const char* path)
		{
			if (path == nullptr)
			{
				mPersistencePath[0] = '\0';
				return;
			}
			strncpy_s(mPersistencePath, kMaxPathLength, path, _TRUNCATE);
		}

		void GameConnectionController::LoadPersistedUrl()
		{
			if (mPersistencePath[0] == '\0')
				return;

			std::ifstream stream(mPersistencePath);
			if (!stream.is_open())
				return;

			Json::Value root;
			Json::CharReaderBuilder builder;
			std::string errors;
			if (!Json::parseFromStream(builder, stream, &root, &errors))
				return;

			const Json::Value& urlVal = root["url"];
			if (urlVal.isString())
				SetUrl(urlVal.asCString());
		}

		void GameConnectionController::SavePersistedUrl() const
		{
			if (mPersistencePath[0] == '\0' || mUrl[0] == '\0')
				return;

			Json::Value root;
			root["url"] = mUrl;

			Json::StreamWriterBuilder writer;
			writer["indentation"] = "  ";
			std::string text = Json::writeString(writer, root);

			std::ofstream out(mPersistencePath, std::ios::trunc);
			if (!out.is_open())
				return;
			out << text;
		}

		void GameConnectionController::RegisterHandlers()
		{
			if (mBridge == nullptr)
				return;

			mBridge->RegisterRequestHandler(kReqConnect,
				[this](const Json::Value& data) -> Json::Value
				{
					return HandleConnectRequest(data);
				});

			mBridge->RegisterRequestHandler(kReqDisconnect,
				[this](const Json::Value& data) -> Json::Value
				{
					return HandleDisconnectRequest(data);
				});

			mBridge->RegisterRequestHandler(kReqGetState,
				[this](const Json::Value& data) -> Json::Value
				{
					return HandleGetStateRequest(data);
				});
		}

		Json::Value GameConnectionController::HandleConnectRequest(const Json::Value& data)
		{
			Json::Value response;

			if (mState != State::kDisconnected)
			{
				response["ok"] = false;
				response["error"] = "Already connected or connecting";
				return response;
			}

			std::string url = data.get("url", "").asString();
			if (url.empty())
			{
				response["ok"] = false;
				response["error"] = "url is required";
				return response;
			}

			SetUrl(url.c_str());
			SetLastError("");

			if (mUseStub)
			{
				BeginConnectStub();
			}
			else
			{
				BeginConnectReal();
			}

			response["ok"] = true;
			return response;
		}

		Json::Value GameConnectionController::HandleDisconnectRequest(const Json::Value& /*data*/)
		{
			Json::Value response;
			DisconnectInternal("user requested");
			response["ok"] = true;
			return response;
		}

		Json::Value GameConnectionController::HandleGetStateRequest(const Json::Value& /*data*/)
		{
			Json::Value response;
			BuildStatePayload(response);
			return response;
		}

		void GameConnectionController::BeginConnectStub()
		{
			mConnectingPending = true;
			mConnectingElapsed = 0.0f;
			TransitionTo(State::kConnecting);
		}

		void GameConnectionController::CompleteConnectStub()
		{
			mHeartbeatElapsed = 0.0f;
			mHeartbeatLastPongMs = 0.0f;
			TransitionTo(State::kConnected);
			SavePersistedUrl();
		}

		void GameConnectionController::BeginConnectReal()
		{
			// Slice 2 will wire this to GameConnectionManager::Connect with
			// a host/port parsed from mUrl. For Slice 1 this path is never
			// taken because mUseStub is always true.
			if (mManager == nullptr)
			{
				SetLastError("GameConnectionManager not available");
				TransitionTo(State::kDisconnected);
				return;
			}
			TransitionTo(State::kConnecting);
		}

		void GameConnectionController::DisconnectInternal(const char* reason)
		{
			mConnectingPending = false;
			mConnectingElapsed = 0.0f;
			mHeartbeatElapsed = 0.0f;

			if (!mUseStub && mManager != nullptr)
				mManager->Disconnect();

			if (reason != nullptr && reason[0] != '\0' &&
				strcmp(reason, "user requested") != 0)
			{
				SetLastError(reason);
			}

			TransitionTo(State::kDisconnected);
		}

		void GameConnectionController::TransitionTo(State newState)
		{
			mState = newState;
			PublishState();
		}

		void GameConnectionController::BuildStatePayload(Json::Value& out) const
		{
			out["state"] = kStateToString(mState);
			out["url"] = mUrl;
			out["lastError"] = mLastError;
			if (mState == State::kConnected)
			{
				out["info"] = mStubGameInfo;
			}
			else
			{
				out["info"] = Json::Value(Json::nullValue);
			}
		}

		void GameConnectionController::PublishState()
		{
			if (mBridge == nullptr)
				return;

			Json::Value payload;
			BuildStatePayload(payload);
			mBridge->NotifyUIDataChanged(kTopicState, payload);
		}

		void GameConnectionController::PublishHeartbeat()
		{
			if (mBridge == nullptr)
				return;

			// In stub mode the "pong" is synthetic — timestamp of the emit.
			time_t now = time(nullptr);
			double nowMs = static_cast<double>(now) * 1000.0;

			Json::Value payload;
			payload["lastPongMs"] = nowMs;
			mBridge->NotifyUIDataChanged(kTopicHeartbeat, payload);
		}

		void GameConnectionController::SetLastError(const char* msg)
		{
			if (msg == nullptr)
			{
				mLastError[0] = '\0';
				return;
			}
			strncpy_s(mLastError, kMaxErrorLength, msg, _TRUNCATE);
		}

		void GameConnectionController::SetUrl(const char* url)
		{
			if (url == nullptr)
			{
				mUrl[0] = '\0';
				return;
			}
			strncpy_s(mUrl, kMaxUrlLength, url, _TRUNCATE);
		}
	}
}
