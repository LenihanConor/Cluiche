#include "DiaDebugServer/DebugServerModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaApplication/ApplicationPhase.h>
#include <DiaApplication/DebugDataTypes.h>
#include <DiaApplication/Metrics/MetricsCollectorModule.h>
#include <DiaWebSocket/Server.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaLogger/DiaLog.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace Dia
{
	namespace DebugServer
	{
		const Dia::Core::StringCRC DebugServerModule::kUniqueId("DebugServerModule");
		const Dia::Core::StringCRC& DebugServerModule::kTypeId = DebugServerModule::kUniqueId;

		DebugServerModule::DebugServerModule(Dia::Application::ProcessingUnit* pu)
			: Module(pu, kUniqueId, RunningEnum::kUpdate)
			, mServer(nullptr)
			, mPort(8080)
			, mAutoStart(true)
			, mMetricsBroadcastInterval(0.5f)
			, mMetricsTimer(0.0f)
			, mStartTimestamp(0)
		{
			mGameName[0] = '\0';
			mGameBuild[0] = '\0';
		}

		void DebugServerModule::SetGameInfo(const char* name, const char* build)
		{
			if (name) strncpy_s(mGameName, sizeof(mGameName), name, _TRUNCATE);
			else mGameName[0] = '\0';
			if (build) strncpy_s(mGameBuild, sizeof(mGameBuild), build, _TRUNCATE);
			else mGameBuild[0] = '\0';
		}

		DebugServerModule::~DebugServerModule()
		{
			if (mServer)
			{
				mServer->Stop();
				delete mServer;
				mServer = nullptr;
			}
		}

		Dia::Application::StateObject::OpertionResponse DebugServerModule::DoStart(const IStartData* /*startData*/)
		{
			DIA_LOG_INFO("DebugServer", "DebugServerModule: DoStart port=%d autoStart=%d", static_cast<int>(mPort), mAutoStart ? 1 : 0);
			mServer = new Dia::WebSocket::Server(mPort);

			mServer->SetConnectionCallback([this](int connId, bool connected) {
				HandleConnection(connId, connected);
			});

			mServer->SetMessageCallback([this](int connId, const Dia::WebSocket::Message& msg) {
				HandleMessage(connId, msg);
			});

			RegisterProtocolCommands();

			GetAssociatedProcessingUnit()->GetMessageBus().Subscribe(
				Dia::Application::DebugDataType::kPhaseTransition,
				kUniqueId,
				[this](const Dia::Application::Message& msg) {
					auto subscribers = mSubscriptionManager.GetSubscribers(
						Dia::Application::DebugDataType::kPhaseTransition);
					if (subscribers.Size() == 0) return;

					Json::Value eventPayload;
					eventPayload["pu_id"] = GetAssociatedProcessingUnit()->GetUniqueId().AsChar();

					Json::Value eventJson = Dia::DebugProtocol::SerializeEvent(
						Dia::Application::DebugDataType::kPhaseTransition, eventPayload);

					std::string jsonStr = Json::FastWriter().write(eventJson);
					for (unsigned int i = 0; i < subscribers.Size(); ++i)
					{
						mServer->SendText(subscribers[i], jsonStr.c_str());
					}
					mStats.messagesSentTotal += static_cast<int>(subscribers.Size());
				}
			);

			if (mAutoStart)
			{
				StartServer();
			}

			mStartTimestamp = Dia::DebugProtocol::GetTimestampNow();

			return StateObject::OpertionResponse::kImmediate;
		}

		void DebugServerModule::DoUpdate()
		{
			if (!mServer || !mServer->IsRunning()) return;

			uint64_t frameStart = Dia::DebugProtocol::GetTimestampNow();

			mServer->Update();

			mMetricsTimer += 0.016f; // ~60fps approximation; real delta time not available on Module
			if (mMetricsTimer >= mMetricsBroadcastInterval)
			{
				if (mServer->GetConnectionCount() > 0)
				{
					BroadcastCoreMetrics();
				}
				mMetricsTimer = 0.0f;
			}

			mStats.connectionCount = mServer->GetConnectionCount();
			mStats.subscriptionCount = mSubscriptionManager.GetSubscriptionCount();

			if (mStartTimestamp > 0)
			{
				uint64_t now = Dia::DebugProtocol::GetTimestampNow();
				mStats.uptimeSeconds = static_cast<int>((now - mStartTimestamp) / 1000000);
			}

			uint64_t frameEnd = Dia::DebugProtocol::GetTimestampNow();
			mStats.debugServerOverheadMs = static_cast<float>(frameEnd - frameStart) / 1000.0f;
		}

		void DebugServerModule::DoStop()
		{
			DIA_LOG_INFO("DebugServer", "DebugServerModule: DoStop");
			if (mServer)
			{
				mServer->Stop();
				delete mServer;
				mServer = nullptr;
			}
		}

		bool DebugServerModule::StartServer()
		{
			if (!mServer) return false;
			if (mServer->IsRunning()) return true;
			bool ok = mServer->Start();
			DIA_LOG_INFO("DebugServer", "DebugServerModule: StartServer on port %d result=%d", static_cast<int>(mPort), ok ? 1 : 0);
			return ok;
		}

		void DebugServerModule::StopServer()
		{
			if (mServer)
			{
				mServer->Stop();
			}
		}

		bool DebugServerModule::IsServerRunning() const
		{
			return mServer && mServer->IsRunning();
		}

		int DebugServerModule::GetConnectionCount() const
		{
			if (!mServer) return 0;
			return mServer->GetConnectionCount();
		}

		void DebugServerModule::HandleConnection(int connId, bool connected)
		{
			DIA_LOG_INFO("DebugServer", "DebugServerModule: HandleConnection connId=%d connected=%d totalConnections=%d", connId, connected ? 1 : 0, mServer ? mServer->GetConnectionCount() : -1);
			if (connected)
			{
				Json::Value welcome = Dia::DebugProtocol::SerializeHandshakeResponse(
					Dia::DebugProtocol::kProtocolVersion,
					true,
					"DiaDebugServer",
					"1.0.0"
				);
				SendJsonToConnection(connId, welcome);

				DIA_LOG_INFO("DebugServer", "DebugServerModule: Sending game_info to connId=%d", connId);
				SendGameInfo(connId);
			}
			else
			{
				DIA_LOG_INFO("DebugServer", "DebugServerModule: Client disconnected connId=%d", connId);
				mSubscriptionManager.UnsubscribeAll(connId);
			}
		}

		void DebugServerModule::SendGameInfo(int connId)
		{
			const char* name = mGameName[0] ? mGameName : "DiaDebugServer";
			const char* build = mGameBuild[0] ? mGameBuild : "unknown";
			Json::Value info = Dia::DebugProtocol::SerializeGameInfo(
				name, build, GetProcessingUnitCount(), GetCurrentPhaseName());
			SendJsonToConnection(connId, info);
		}

		const char* DebugServerModule::GetCurrentPhaseName() const
		{
			const Dia::Application::ProcessingUnit* pu = GetAssociatedProcessingUnit();
			if (pu == nullptr) return "";
			const Dia::Application::Phase* phase = pu->GetCurrentPhase();
			if (phase == nullptr) return "";
			return phase->GetUniqueId().AsChar();
		}

		int DebugServerModule::GetProcessingUnitCount() const
		{
			// DiaDebugServer runs inside a single ProcessingUnit and has no global view
			// of the application's PU graph; report 1 (self) until the application
			// wires a PU-count provider through SetGameInfo.
			return 1;
		}

		void DebugServerModule::HandleMessage(int connId, const Dia::WebSocket::Message& msg)
		{
			if (msg.type != Dia::WebSocket::MessageType::kText) return;

			mStats.messagesReceivedTotal++;

			Json::Value json;
			Json::Reader reader;
			if (!reader.parse(msg.AsText(), msg.AsText() + msg.length, json))
			{
				DIA_LOG_ERROR("DebugServer", "DebugServerModule: HandleMessage connId=%d JSON parse error", connId);
				Json::Value errorJson = Dia::DebugProtocol::SerializeError("parse_error", "Invalid JSON");
				SendJsonToConnection(connId, errorJson);
				return;
			}

			Dia::DebugProtocol::MessageType type = Dia::DebugProtocol::GetMessageType(json);
			DIA_LOG_DEBUG("DebugServer", "DebugServerModule: HandleMessage connId=%d type=%d", connId, static_cast<int>(type));

			switch (type)
			{
			case Dia::DebugProtocol::MessageType::kSubscribe:
				HandleSubscribe(connId, json);
				break;
			case Dia::DebugProtocol::MessageType::kUnsubscribe:
				HandleUnsubscribe(connId, json);
				break;
			case Dia::DebugProtocol::MessageType::kCommandRequest:
				HandleCommand(connId, json);
				break;
			case Dia::DebugProtocol::MessageType::kHandshake:
				HandleHandshake(connId, json);
				break;
			case Dia::DebugProtocol::MessageType::kPing:
				HandlePing(connId, json);
				break;
			default:
			{
				Json::Value errorJson = Dia::DebugProtocol::SerializeError("unknown_type", "Unknown message type");
				SendJsonToConnection(connId, errorJson);
				break;
			}
			}
		}

		void DebugServerModule::HandleSubscribe(int connId, const Json::Value& json)
		{
			Dia::DebugProtocol::SubscribeMessage subMsg;
			if (!Dia::DebugProtocol::ParseSubscribe(json, subMsg))
			{
				Json::Value errorJson = Dia::DebugProtocol::SerializeError("invalid_subscribe", "Missing data_type");
				SendJsonToConnection(connId, errorJson);
				return;
			}

			mSubscriptionManager.Subscribe(connId, subMsg.dataType, subMsg.filter);
		}

		void DebugServerModule::HandleUnsubscribe(int connId, const Json::Value& json)
		{
			if (!json.isMember("data_type") || !json["data_type"].isString()) return;

			Dia::Core::StringCRC dataType(json["data_type"].asCString());
			mSubscriptionManager.Unsubscribe(connId, dataType);
		}

		void DebugServerModule::HandleCommand(int connId, const Json::Value& json)
		{
			Dia::DebugProtocol::CommandRequestMessage cmdMsg;
			if (!Dia::DebugProtocol::ParseCommandRequest(json, cmdMsg))
			{
				Json::Value errorJson = Dia::DebugProtocol::SerializeError("invalid_command", "Missing command field");
				SendJsonToConnection(connId, errorJson);
				return;
			}

			if (mCommandDispatcher.IsProtocolCommand(cmdMsg.command))
			{
				Json::Value response = mCommandDispatcher.ExecuteProtocolCommand(
					cmdMsg.command, cmdMsg.payload, this);
				SendJsonToConnection(connId, response);
			}
			else
			{
				Json::Value response = mCommandDispatcher.ExecuteDiaAPICommand(
					cmdMsg.command, cmdMsg.payload);
				SendJsonToConnection(connId, response);
			}
		}

		void DebugServerModule::HandleHandshake(int connId, const Json::Value& json)
		{
			Dia::DebugProtocol::HandshakeRequest request;
			if (!Dia::DebugProtocol::ParseHandshakeRequest(json, request))
			{
				Json::Value errorJson = Dia::DebugProtocol::SerializeError("invalid_handshake", "Invalid handshake");
				SendJsonToConnection(connId, errorJson);
				return;
			}

			bool accepted = (request.protocolVersion == Dia::DebugProtocol::kProtocolVersion);
			Json::Value response = Dia::DebugProtocol::SerializeHandshakeResponse(
				Dia::DebugProtocol::kProtocolVersion,
				accepted,
				"DiaDebugServer",
				"1.0.0"
			);
			SendJsonToConnection(connId, response);
		}

		void DebugServerModule::HandlePing(int connId, const Json::Value& json)
		{
			Dia::DebugProtocol::PingMessage ping;
			if (!Dia::DebugProtocol::ParsePing(json, ping))
			{
				DIA_LOG_WARNING("DebugServer", "DebugServerModule: HandlePing connId=%d invalid ping", connId);
				Json::Value errorJson = Dia::DebugProtocol::SerializeError("invalid_ping", "Missing ts");
				SendJsonToConnection(connId, errorJson);
				return;
			}
			DIA_LOG_DEBUG("DebugServer", "DebugServerModule: HandlePing connId=%d ts=%llu, sending pong", connId, static_cast<unsigned long long>(ping.ts));
			Json::Value pong = Dia::DebugProtocol::SerializePong(ping.ts);
			SendJsonToConnection(connId, pong);
		}

		void DebugServerModule::BroadcastCoreMetrics()
		{
			uint64_t serializeStart = Dia::DebugProtocol::GetTimestampNow();

			Dia::DebugProtocol::CoreMetricsPayload metrics;
			memset(&metrics, 0, sizeof(metrics));

			Dia::Application::MetricsCollectorModule* collector = GetAssociatedProcessingUnit()->GetMetricsCollector();
			if (collector != nullptr)
			{
				const Dia::Application::MetricsSnapshot& snap = collector->GetSnapshot();
				metrics.fps = (snap.puCount > 0) ? snap.puMetrics[0].fps : 0.0f;
				metrics.frameTimeMs = (snap.puCount > 0) ? snap.puMetrics[0].frameTimeMs : 0.0f;
				metrics.memoryUsedMb = snap.memoryUsedMB;
				metrics.uptimeSeconds = snap.uptimeSeconds;
				metrics.puCount = snap.puCount;
				for (unsigned int i = 0; i < snap.puCount && i < Dia::DebugProtocol::CoreMetricsPayload::kMaxProcessingUnits; ++i)
				{
					strncpy_s(metrics.puMetrics[i].name, sizeof(metrics.puMetrics[i].name), snap.puMetrics[i].name, _TRUNCATE);
					metrics.puMetrics[i].fps = snap.puMetrics[i].fps;
					metrics.puMetrics[i].frameTimeMs = snap.puMetrics[i].frameTimeMs;
				}
			}

#ifdef _WIN32
			if (collector == nullptr)
			{
				PROCESS_MEMORY_COUNTERS pmc;
				if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
				{
					metrics.memoryUsedMb = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
				}
			}

			MEMORYSTATUSEX memStatus;
			memStatus.dwLength = sizeof(memStatus);
			if (GlobalMemoryStatusEx(&memStatus))
			{
				metrics.memoryAvailableMb = static_cast<float>(memStatus.ullAvailPhys) / (1024.0f * 1024.0f);
			}
#endif

			Json::Value json = Dia::DebugProtocol::SerializeCoreMetrics(metrics);
			json["payload"]["debug_server_overhead_ms"] = mStats.debugServerOverheadMs;

			uint64_t serializeEnd = Dia::DebugProtocol::GetTimestampNow();
			mStats.serializationTimeMs = static_cast<float>(serializeEnd - serializeStart) / 1000.0f;

			uint64_t broadcastStart = Dia::DebugProtocol::GetTimestampNow();

			std::string jsonStr = Json::FastWriter().write(json);
			mServer->BroadcastText(jsonStr.c_str());

			uint64_t broadcastEnd = Dia::DebugProtocol::GetTimestampNow();
			mStats.broadcastTimeMs = static_cast<float>(broadcastEnd - broadcastStart) / 1000.0f;

			mStats.messagesSentTotal++;
			mStats.averageMessageSizeBytes = static_cast<int>(jsonStr.size());
			if (mMetricsBroadcastInterval > 0.0f)
			{
				mStats.bytesSentPerSec = static_cast<float>(jsonStr.size()) / mMetricsBroadcastInterval;
			}
		}

		void DebugServerModule::NotifySubscribers(const Dia::Core::StringCRC& dataType, const Json::Value& payload)
		{
			auto subscribers = mSubscriptionManager.GetSubscribers(dataType);
			if (subscribers.Size() == 0) return;

			Json::Value json = Dia::DebugProtocol::SerializeDataUpdate(dataType, payload);
			std::string jsonStr = Json::FastWriter().write(json);

			for (unsigned int i = 0; i < subscribers.Size(); ++i)
			{
				mServer->SendText(subscribers[i], jsonStr.c_str());
			}
			mStats.messagesSentTotal += static_cast<int>(subscribers.Size());
		}

		void DebugServerModule::RegisterProtocolCommands()
		{
			mCommandDispatcher.RegisterProtocolCommand(
				Dia::Core::StringCRC("get_state"),
				[this](const Json::Value& payload, Json::Value& responseOut) {
					const Dia::Application::ProcessingUnit* pu = GetAssociatedProcessingUnit();
					responseOut["type"] = Dia::DebugProtocol::MessageTypeToString(Dia::DebugProtocol::MessageType::kCommandResponse);
					responseOut["command"] = "get_state";
					responseOut["success"] = true;

					Json::Value statePayload;
					if (pu)
					{
						statePayload["processing_unit"] = StateSerializer::SerializeProcessingUnitState(pu);
						const Dia::Application::Phase* currentPhase = pu->GetCurrentPhase();
						if (currentPhase)
						{
							statePayload["current_phase"] = StateSerializer::SerializePhaseState(currentPhase);
						}
					}
					responseOut["payload"] = statePayload;
				}
			);

			mCommandDispatcher.RegisterProtocolCommand(
				Dia::Core::StringCRC("list_commands"),
				[this](const Json::Value& /*payload*/, Json::Value& responseOut) {
					responseOut["type"] = Dia::DebugProtocol::MessageTypeToString(Dia::DebugProtocol::MessageType::kCommandResponse);
					responseOut["command"] = "list_commands";
					responseOut["success"] = true;

					Json::Value commandsPayload;

					Json::Value protocolCmds(Json::arrayValue);
					protocolCmds.append("get_state");
					protocolCmds.append("list_commands");
					protocolCmds.append("get_server_stats");
					commandsPayload["protocol_commands"] = protocolCmds;

					Json::Value apiCmds(Json::arrayValue);
					auto commands = Dia::API::ListCommands();
					for (unsigned int i = 0; i < commands.Size(); ++i)
					{
						apiCmds.append(commands[i]->name.AsChar());
					}
					commandsPayload["api_commands"] = apiCmds;

					responseOut["payload"] = commandsPayload;
				}
			);

			mCommandDispatcher.RegisterProtocolCommand(
				Dia::Core::StringCRC("get_server_stats"),
				[this](const Json::Value& /*payload*/, Json::Value& responseOut) {
					responseOut["type"] = Dia::DebugProtocol::MessageTypeToString(Dia::DebugProtocol::MessageType::kCommandResponse);
					responseOut["command"] = "get_server_stats";
					responseOut["success"] = true;

					Json::Value statsPayload;

					statsPayload["game_side"]["debug_server_overhead_ms"] = mStats.debugServerOverheadMs;
					statsPayload["game_side"]["serialization_time_ms"] = mStats.serializationTimeMs;
					statsPayload["game_side"]["broadcast_time_ms"] = mStats.broadcastTimeMs;
					statsPayload["game_side"]["subscription_count"] = mStats.subscriptionCount;

					statsPayload["editor_side"]["bytes_sent_per_sec"] = mStats.bytesSentPerSec;
					statsPayload["editor_side"]["message_queue_size"] = mStats.messageQueueSize;
					statsPayload["editor_side"]["messages_dropped"] = mStats.messagesDropped;
					statsPayload["editor_side"]["average_message_size_bytes"] = mStats.averageMessageSizeBytes;

					statsPayload["server"]["connection_count"] = mStats.connectionCount;
					statsPayload["server"]["messages_sent_total"] = mStats.messagesSentTotal;
					statsPayload["server"]["messages_received_total"] = mStats.messagesReceivedTotal;
					statsPayload["server"]["uptime_seconds"] = mStats.uptimeSeconds;

					responseOut["payload"] = statsPayload;
				}
			);
		}

		void DebugServerModule::SendJsonToConnection(int connId, const Json::Value& json)
		{
			std::string str = Json::FastWriter().write(json);
			mServer->SendText(connId, str.c_str());
			mStats.messagesSentTotal++;
		}

		void DebugServerModule::BroadcastJson(const Json::Value& json)
		{
			std::string str = Json::FastWriter().write(json);
			mServer->BroadcastText(str.c_str());
			mStats.messagesSentTotal++;
		}
	}
}

#include <DiaApplication/TypeRegistry/RegistrationMacros.h>

// windows.h (included above) defines SetPort as a macro that expands to
// SetPortA / SetPortW. Drop the macro so we can call the real method.
#ifdef SetPort
#undef SetPort
#endif

namespace { using _DebugServerModule = Dia::DebugServer::DebugServerModule; }
DIA_REGISTER_MODULE(_DebugServerModule) {
	Dia::DebugServer::DebugServerModule* mod = new Dia::DebugServer::DebugServerModule(pu);
	if (config.isMember("port") && config["port"].isInt())
		mod->SetPort(static_cast<uint16_t>(config["port"].asInt()));
	if (config.isMember("auto_start") && config["auto_start"].isBool())
		mod->EnableAutoStart(config["auto_start"].asBool());
	const char* gameName = config.isMember("game_name") && config["game_name"].isString()
		? config["game_name"].asCString() : "";
	const char* gameBuild = config.isMember("game_build") && config["game_build"].isString()
		? config["game_build"].asCString() : "";
	mod->SetGameInfo(gameName, gameBuild);
	return mod;
}

