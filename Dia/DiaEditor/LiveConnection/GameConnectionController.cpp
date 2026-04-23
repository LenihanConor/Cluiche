#include "DiaEditor/LiveConnection/GameConnectionController.h"

#include <DiaDebugProtocol/DiaDebugProtocol.h>

#include "DiaEditor/LiveConnection/GameConnectionManager.h"
#include "DiaEditor/MVC/EditorView.h"
#include "DiaEditor/UI/WebUIBridge.h"

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String1024.h>

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
		const float GameConnectionController::kHandshakeTimeoutSeconds = 5.0f;
		const float GameConnectionController::kPongTimeoutSeconds = 20.0f;

		static const Dia::Core::StringCRC kReqConnect("game_connection.connect");
		static const Dia::Core::StringCRC kReqDisconnect("game_connection.disconnect");
		static const Dia::Core::StringCRC kReqGetState("game_connection.get_state");

		static const char* kTopicState = "game_connection";
		static const char* kTopicHeartbeat = "game_connection_heartbeat";
		static const char* kTopicCoreMetrics = "core_metrics";

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
			, mEditorView(nullptr)
			, mUseStub(false)
			, mConnectingPending(false)
			, mConnectingElapsed(0.0f)
			, mHeartbeatElapsed(0.0f)
			, mHeartbeatLastPongMs(0.0f)
			, mGameInfoReceived(false)
			, mHandshakeElapsed(0.0f)
			, mLastPingSentTs(0)
			, mLastPongReceivedTs(0)
			, mSinceLastPong(0.0f)
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

		void GameConnectionController::Initialize(WebUIBridge* bridge, GameConnectionManager* manager, EditorView* editorView)
		{
			DIA_LOG_INFO("Editor", "GameConnectionController: Initialize bridge=%p manager=%p", bridge, manager);
			mBridge = bridge;
			mManager = manager;
			mEditorView = editorView;

			if (mManager != nullptr)
			{
				mManager->SetConnectionCallback([this](bool connected) { OnManagerConnection(connected); });
				mManager->SetRawMessageCallback([this](const char* rawText, unsigned int /*rawLength*/, const Json::Value& envelope) { OnManagerRawMessage(rawText, envelope); });
			}

			RegisterHandlers();
		}

		void GameConnectionController::Shutdown()
		{
			DIA_LOG_INFO("Editor", "GameConnectionController: Shutdown");
			mBridge = nullptr;
			mManager = nullptr;
		}

		void GameConnectionController::Update(float deltaTime)
		{
			if (mState == State::kConnecting)
			{
				if (mUseStub && mConnectingPending)
				{
					mConnectingElapsed += deltaTime;
					if (mConnectingElapsed >= kStubConnectDelaySeconds)
					{
						mConnectingPending = false;
						CompleteConnectStub();
					}
				}
				else if (!mUseStub)
				{
					mHandshakeElapsed += deltaTime;
					// Q15: if game_info doesn't arrive within the timeout
					// budget, tear the connection down with a clear error.
					if (!mGameInfoReceived && mHandshakeElapsed >= kHandshakeTimeoutSeconds)
					{
						DisconnectInternal("Handshake timeout");
					}
				}
			}
			else if (mState == State::kConnected)
			{
				mHeartbeatElapsed += deltaTime;
				mSinceLastPong += deltaTime;

				if (mHeartbeatElapsed >= kHeartbeatIntervalSeconds)
				{
					mHeartbeatElapsed = 0.0f;
					if (mUseStub)
					{
						mHeartbeatLastPongMs = 0.0f;
						PublishHeartbeat();
					}
					else if (mManager != nullptr)
					{
						time_t now = time(nullptr);
						mLastPingSentTs = static_cast<uint64_t>(now) * 1000ULL;

						dia::debug::DebugMessage pingMsg;
						pingMsg.set_type(dia::debug::MESSAGE_TYPE_PING);
						pingMsg.mutable_ping()->set_ts(mLastPingSentTs);

						char pingBuffer[256];
						if (Dia::Proto::ToJson(pingMsg, pingBuffer, sizeof(pingBuffer)))
							mManager->SendRawText(pingBuffer);

						// Reset the pong timer on send so the 20s timeout is
						// measured from the last ping, not from connection start.
						mSinceLastPong = 0.0f;
					}
				}

				// Heartbeat timeout: no pong within 2x the interval → drop.
				if (!mUseStub && mSinceLastPong >= kPongTimeoutSeconds)
				{
					DisconnectInternal("Heartbeat timeout");
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

		void GameConnectionController::AutoConnect(const char* url)
		{
			if (mState != State::kDisconnected || url == nullptr || url[0] == '\0')
				return;
			DIA_LOG_INFO("Editor", "GameConnectionController: AutoConnect to '%s'", url);
			SetUrl(url);
			SetLastError("");
			BeginConnectReal();
		}

		void GameConnectionController::LoadPersistedUrl()
		{
			if (mPersistencePath[0] == '\0')
				return;

			std::ifstream stream(mPersistencePath);
			if (!stream.is_open())
			{
				DIA_LOG_INFO("Editor", "GameConnectionController: No persisted URL found at '%s'", mPersistencePath);
				return;
			}

			Json::Value root;
			Json::CharReaderBuilder builder;
			std::string errors;
			if (!Json::parseFromStream(builder, stream, &root, &errors))
				return;

			const Json::Value& urlVal = root["url"];
			if (urlVal.isString())
			{
				SetUrl(urlVal.asCString());
				DIA_LOG_INFO("Editor", "GameConnectionController: Loaded persisted URL '%s'", mUrl);
			}
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
			{
				DIA_LOG_WARNING("Editor", "GameConnectionController: RegisterHandlers skipped, bridge is null");
				return;
			}

			DIA_LOG_INFO("Editor", "GameConnectionController: Registering request handlers (connect, disconnect, get_state)");
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
			DIA_LOG_INFO("Editor", "GameConnectionController: HandleConnectRequest received");
			Json::Value response;

			if (mState != State::kDisconnected)
			{
				DIA_LOG_WARNING("Editor", "GameConnectionController: Connect rejected, already in state %s", kStateToString(mState));
				response["ok"] = false;
				response["error"] = "Already connected or connecting";
				return response;
			}

			std::string url = data.get("url", "").asString();
			if (url.empty())
			{
				DIA_LOG_WARNING("Editor", "GameConnectionController: Connect rejected, url is empty");
				response["ok"] = false;
				response["error"] = "url is required";
				return response;
			}

			DIA_LOG_INFO("Editor", "GameConnectionController: Connecting to %s (stub=%d)", url.c_str(), mUseStub ? 1 : 0);
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
			DIA_LOG_INFO("Editor", "GameConnectionController: HandleDisconnectRequest");
			Json::Value response;
			DisconnectInternal("user requested");
			response["ok"] = true;
			return response;
		}

		Json::Value GameConnectionController::HandleGetStateRequest(const Json::Value& /*data*/)
		{
			DIA_LOG_DEBUG("Editor", "GameConnectionController: HandleGetStateRequest state=%s", kStateToString(mState));
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
			if (mManager == nullptr)
			{
				DIA_LOG_ERROR("Editor", "GameConnectionController: BeginConnectReal failed, manager is null");
				SetLastError("GameConnectionManager not available");
				TransitionTo(State::kDisconnected);
				return;
			}

			char host[128];
			int port = 0;
			if (!ParseWebSocketUrl(mUrl, host, sizeof(host), port))
			{
				DIA_LOG_ERROR("Editor", "GameConnectionController: BeginConnectReal failed, invalid URL '%s'", mUrl);
				SetLastError("Invalid WebSocket URL (expected ws://host:port)");
				TransitionTo(State::kDisconnected);
				return;
			}

			DIA_LOG_INFO("Editor", "GameConnectionController: BeginConnectReal host=%s port=%d", host, port);

			mGameInfoReceived = false;
			mHandshakeElapsed = 0.0f;
			mSinceLastPong = 0.0f;
			mGameInfo = Json::Value(Json::nullValue);

			TransitionTo(State::kConnecting);
			mManager->Connect(host, port);
		}

		void GameConnectionController::OnManagerConnection(bool connected)
		{
			DIA_LOG_INFO("Editor", "GameConnectionController: OnManagerConnection connected=%d currentState=%s", connected ? 1 : 0, kStateToString(mState));

			if (connected)
			{
				return;
			}

			if (mState != State::kDisconnected)
			{
				DIA_LOG_WARNING("Editor", "GameConnectionController: Socket dropped while in state %s", kStateToString(mState));
				if (mLastError[0] == '\0')
					SetLastError("Connection lost");
				PushGameConsoleEntry("error", "Connection lost");
				TransitionTo(State::kDisconnected);
			}
		}

		void GameConnectionController::OnManagerRawMessage(const char* rawText, const Json::Value& /*envelope*/)
		{
			dia::debug::DebugMessage msg;
			if (!Dia::Proto::FromJson(rawText, &msg))
			{
				DIA_LOG_WARNING("Editor", "GameConnectionController: OnManagerRawMessage proto parse failed");
				return;
			}

			switch (msg.payload_case())
			{
			case dia::debug::DebugMessage::kGameInfo:
			{
				const auto& info = msg.game_info();
				mGameInfo["name"] = info.name();
				mGameInfo["build"] = info.build();
				mGameInfo["processingUnitCount"] = info.processing_unit_count();
				mGameInfo["currentPhase"] = info.current_phase();
				mGameInfoReceived = true;

				if (mState == State::kConnecting)
				{
					mHeartbeatElapsed = 0.0f;
					mSinceLastPong = 0.0f;
					TransitionTo(State::kConnected);
					SavePersistedUrl();

					DIA_LOG_INFO("Editor", "GameConnectionController: Handshake complete, connected to %s (build %s)",
						info.name().c_str(), info.build().c_str());

					char logMsg[256];
					snprintf(logMsg, sizeof(logMsg), "Connected to %s (build %s)",
						info.name().c_str(), info.build().c_str());
					PushGameConsoleEntry("info", logMsg);
				}
				break;
			}
			case dia::debug::DebugMessage::kPong:
			{
				const auto& pong = msg.pong();
				mSinceLastPong = 0.0f;
				mLastPongReceivedTs = pong.ts();
				mHeartbeatLastPongMs = static_cast<float>(pong.ts());
				PublishHeartbeat();
				break;
			}
			case dia::debug::DebugMessage::kLog:
			{
				const auto& log = msg.log();
				DIA_LOG_INFO("Editor", "GameConnectionController: Received game log [%s] %s", log.level().c_str(), log.message().c_str());
				PushGameConsoleEntry(log.level().c_str(), log.message().c_str());
				break;
			}
			case dia::debug::DebugMessage::kLogBatch:
			{
				const auto& batch = msg.log_batch();
				for (int i = 0; i < batch.entries_size(); ++i)
				{
					const auto& e = batch.entries(i);
					PushGameConsoleEntry(e.level().c_str(), e.message().c_str());
				}
				break;
			}
			case dia::debug::DebugMessage::kCoreMetrics:
			{
				if (mBridge != nullptr)
				{
					Json::Value metricsPayload;
					const auto& cm = msg.core_metrics();
					metricsPayload["fps"] = cm.fps();
					metricsPayload["frame_time_ms"] = cm.frame_time_ms();
					metricsPayload["memory_used_mb"] = cm.memory_used_mb();
					metricsPayload["memory_available_mb"] = cm.memory_available_mb();
					metricsPayload["uptime_seconds"] = cm.uptime_seconds();

					Json::Value puArray(Json::arrayValue);
					for (int i = 0; i < cm.processing_units_size(); ++i)
					{
						Json::Value pu;
						pu["name"] = cm.processing_units(i).name();
						pu["fps"] = cm.processing_units(i).fps();
						pu["frame_time_ms"] = cm.processing_units(i).frame_time_ms();
						puArray.append(pu);
					}
					metricsPayload["processing_units"] = puArray;

					mBridge->NotifyUIDataChanged(kTopicCoreMetrics, metricsPayload);
				}
				break;
			}
			default:
				break;
			}
		}

		bool GameConnectionController::ParseWebSocketUrl(const char* url, char* outHost, unsigned int hostLen, int& outPort)
		{
			if (url == nullptr || outHost == nullptr || hostLen == 0)
				return false;

			const char* p = url;
			if (strncmp(p, "ws://", 5) == 0)
				p += 5;
			else if (strncmp(p, "wss://", 6) == 0)
				p += 6;

			// host is everything up to ':' or '/'; port follows ':'.
			const char* hostStart = p;
			const char* hostEnd = p;
			while (*hostEnd != '\0' && *hostEnd != ':' && *hostEnd != '/')
				++hostEnd;

			size_t hlen = static_cast<size_t>(hostEnd - hostStart);
			if (hlen == 0 || hlen >= hostLen)
				return false;

			memcpy(outHost, hostStart, hlen);
			outHost[hlen] = '\0';

			if (*hostEnd == ':')
			{
				const char* portStart = hostEnd + 1;
				char portBuf[16];
				unsigned int i = 0;
				while (portStart[i] != '\0' && portStart[i] != '/' && i < sizeof(portBuf) - 1)
				{
					portBuf[i] = portStart[i];
					++i;
				}
				portBuf[i] = '\0';
				outPort = atoi(portBuf);
			}
			else
			{
				outPort = 80;
			}

			return outPort > 0;
		}

		void GameConnectionController::DisconnectInternal(const char* reason)
		{
			DIA_LOG_INFO("Editor", "GameConnectionController: DisconnectInternal reason='%s'", reason ? reason : "(null)");
			mConnectingPending = false;
			mConnectingElapsed = 0.0f;
			mHeartbeatElapsed = 0.0f;

			if (!mUseStub && mManager != nullptr)
				mManager->Disconnect();

			if (reason != nullptr && reason[0] != '\0' &&
				strcmp(reason, "user requested") != 0)
			{
				SetLastError(reason);
				PushGameConsoleEntry("warning", reason);
			}

			TransitionTo(State::kDisconnected);
		}

		void GameConnectionController::TransitionTo(State newState)
		{
			DIA_LOG_INFO("Editor", "GameConnectionController: State %s -> %s", kStateToString(mState), kStateToString(newState));
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
				out["info"] = mUseStub ? mStubGameInfo : mGameInfo;
			}
			else
			{
				out["info"] = Json::Value(Json::nullValue);
			}
		}

		void GameConnectionController::PublishState()
		{
			if (mBridge == nullptr)
			{
				DIA_LOG_WARNING("Editor", "GameConnectionController: PublishState skipped, bridge is null");
				return;
			}

			Json::Value payload;
			BuildStatePayload(payload);
			DIA_LOG_INFO("Editor", "GameConnectionController: PublishState topic='%s' state=%s", kTopicState, kStateToString(mState));
			mBridge->NotifyUIDataChanged(kTopicState, payload);
		}

		void GameConnectionController::PublishHeartbeat()
		{
			if (mBridge == nullptr)
				return;

			// Stub mode emits a synthetic timestamp on a timer; the real path
			// emits the server-supplied pong timestamp.
			double nowMs = 0.0;
			if (mUseStub)
			{
				time_t now = time(nullptr);
				nowMs = static_cast<double>(now) * 1000.0;
			}
			else
			{
				nowMs = static_cast<double>(mLastPongReceivedTs);
			}

			Json::Value payload;
			payload["lastPongMs"] = nowMs;
			mBridge->NotifyUIDataChanged(kTopicHeartbeat, payload);
		}

		void GameConnectionController::PushGameConsoleEntry(const char* level, const char* message)
		{
			if (mEditorView != nullptr)
				mEditorView->PushConsoleEntry(level, message, "game");
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
