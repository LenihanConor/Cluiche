#include "DiaDebugServer/DebugServerModule.h"

#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaApplicationFlow/IApplicationControl.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaWebSocket/Server.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaLogger/DiaLog.h>
#include <DiaLogger/Logger.h>
#include <DiaLogger/LogLevel.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
// windows.h defines SetPort as a macro that expands to SetPortA/W.
// Drop it so we can call the real method.
#ifdef SetPort
#undef SetPort
#endif
#endif

namespace
{
	// Sentinel data-type for stage-transition subscribers (mirrors the v1
	// DebugDataType::kPhaseTransition value).
	static const Dia::Core::StringCRC kStageTransitionTopic("stage_transition");
}

namespace Dia
{
	namespace DebugServer
	{
		const Dia::Core::StringCRC DebugServerModule::kTypeId("DebugServerModule");

		DebugServerModule::DebugServerModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
			, mServer(nullptr)
			, mPort(8080)
			, mAutoStart(true)
			, mMetricsBroadcastInterval(0.5f)
			, mMetricsTimer(0.0f)
			, mFrameTimeAccumMs(0.0f)
			, mFrameTimeSampleCount(0)
			, mLastFpsSample(0.0f)
			, mLastFrameTimeMsSample(0.0f)
			, mStartTimestamp(0)
		{
			mGameName[0] = '\0';
			mGameBuild[0] = '\0';
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

		void DebugServerModule::SetGameInfo(const char* name, const char* build)
		{
			if (name) strncpy_s(mGameName, sizeof(mGameName), name, _TRUNCATE);
			else mGameName[0] = '\0';
			if (build) strncpy_s(mGameBuild, sizeof(mGameBuild), build, _TRUNCATE);
			else mGameBuild[0] = '\0';
		}

		//---------------------------------------------------------------------
		// v2 lifecycle
		//---------------------------------------------------------------------

		void DebugServerModule::OnConfigure(const char* configJson)
		{
			if (configJson == nullptr || configJson[0] == '\0')
				return;

			Json::Value config;
			Json::Reader reader;
			if (!reader.parse(configJson, config))
			{
				DIA_LOG_WARNING("DebugServer", "DebugServerModule: OnConfigure — JSON parse failed, using defaults");
				return;
			}

			if (config.isMember("port") && config["port"].isInt())
				mPort = static_cast<uint16_t>(config["port"].asInt());
			if (config.isMember("auto_start") && config["auto_start"].isBool())
				mAutoStart = config["auto_start"].asBool();

			const char* gameName = config.isMember("game_name") && config["game_name"].isString()
				? config["game_name"].asCString() : "";
			const char* gameBuild = config.isMember("game_build") && config["game_build"].isString()
				? config["game_build"].asCString() : "";
			SetGameInfo(gameName, gameBuild);

			if (config.isMember("log_level") && config["log_level"].isString())
				SetLogSinkLevel(Dia::Logger::LogLevelFromString(config["log_level"].asCString()));
		}

		Dia::ApplicationFlow::StartResult DebugServerModule::DoStart()
		{
			DIA_LOG_INFO("DebugServer", "DebugServerModule: DoStart port=%d autoStart=%d",
				static_cast<int>(mPort), mAutoStart ? 1 : 0);
			mServer = new Dia::WebSocket::Server(mPort);

			mServer->SetConnectionCallback([this](int connId, bool connected) {
				HandleConnection(connId, connected);
			});

			mServer->SetMessageCallback([this](int connId, const Dia::WebSocket::Message& msg) {
				HandleMessage(connId, msg);
			});

			RegisterProtocolCommands();

			// Initialize last-observed stage so the first DoUpdate doesn't
			// fire a spurious "transition from 0 to current".
			if (auto* app = GetApplication())
				mLastObservedStage = app->GetCurrentStage();

			if (mAutoStart)
			{
				StartServer();
			}

			mLogSink.SetServer(mServer);
			Dia::Logger::Logger::Instance().RegisterSink(&mLogSink);

			mStartTimestamp = Dia::DebugProtocol::GetTimestampNow();

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void DebugServerModule::DoUpdate(float deltaTime)
		{
			if (!mServer || !mServer->IsRunning()) return;

			uint64_t frameStart = Dia::DebugProtocol::GetTimestampNow();

			mLogSink.FlushToServer();
			mServer->Update();

			// --- Accumulate frame-time for metrics. -------------------------
			const float deltaMs = deltaTime * 1000.0f;
			mFrameTimeAccumMs    += deltaMs;
			mFrameTimeSampleCount++;

			// --- Poll for stage transitions. --------------------------------
			// v2 has no MessageBus — observe stage changes by comparing the
			// current stage to what we saw last DoUpdate and broadcasting on
			// change to any 'stage_transition' subscribers.
			if (auto* app = GetApplication())
			{
				const Dia::Core::StringCRC now = app->GetCurrentStage();
				if (now != mLastObservedStage)
				{
					BroadcastStageTransition(mLastObservedStage, now);
					mLastObservedStage = now;
				}
			}

			// --- Periodic core-metrics broadcast. ---------------------------
			mMetricsTimer += deltaTime;
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

		Dia::ApplicationFlow::StopResult DebugServerModule::DoStop()
		{
			Dia::Logger::Logger::Instance().UnregisterSink(&mLogSink);
			mLogSink.SetServer(nullptr);

			DIA_LOG_INFO("DebugServer", "DebugServerModule: DoStop");
			if (mServer)
			{
				mServer->Stop();
				delete mServer;
				mServer = nullptr;
			}
			return Dia::ApplicationFlow::StopResult::kDone;
		}

		//---------------------------------------------------------------------
		// Server control
		//---------------------------------------------------------------------

		bool DebugServerModule::StartServer()
		{
			if (!mServer) return false;
			if (mServer->IsRunning()) return true;
			bool ok = mServer->Start();
			DIA_LOG_INFO("DebugServer", "DebugServerModule: StartServer on port %d result=%d",
				static_cast<int>(mPort), ok ? 1 : 0);
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

		//---------------------------------------------------------------------
		// Connection / message handling
		//---------------------------------------------------------------------

		void DebugServerModule::HandleConnection(int connId, bool connected)
		{
			DIA_LOG_INFO("DebugServer",
				"DebugServerModule: HandleConnection connId=%d connected=%d totalConnections=%d",
				connId, connected ? 1 : 0, mServer ? mServer->GetConnectionCount() : -1);
			if (connected)
			{
				dia::debug::DebugMessage welcome;
				welcome.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
				welcome.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
				auto* resp = welcome.mutable_handshake_response();
				resp->set_protocol_version(Dia::DebugProtocol::kProtocolVersion);
				resp->set_accepted(true);
				resp->set_server_name("DiaDebugServer");
				resp->set_server_version("2.0.0");
				SendProtoMessage(connId, welcome);

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
			const char* name  = mGameName[0]  ? mGameName  : "DiaDebugServer";
			const char* build = mGameBuild[0] ? mGameBuild : "unknown";

			dia::debug::DebugMessage msg;
			msg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
			msg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* info = msg.mutable_game_info();
			info->set_name(name);
			info->set_build(build);
			info->set_processing_unit_count(GetProcessingUnitCount());
			info->set_current_phase(GetCurrentStageName());
			SendProtoMessage(connId, msg);
		}

		const char* DebugServerModule::GetCurrentStageName() const
		{
			const auto* app = GetApplication();
			if (app == nullptr) return "";
			return app->GetCurrentStage().AsChar();
		}

		int DebugServerModule::GetProcessingUnitCount() const
		{
			// Ask the introspection interface.  Application implements both
			// IApplicationControl and IApplicationInspectable, but
			// GetApplication() returns the narrow control view — do a
			// dynamic_cast to reach the inspectable interface.
			auto* app = GetApplication();
			if (app == nullptr) return 0;
			auto* insp = dynamic_cast<const Dia::ApplicationFlow::IApplicationInspectable*>(app);
			if (insp == nullptr) return 0;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
			insp->GetProcessingUnits(puIds);
			return static_cast<int>(puIds.Size());
		}

		void DebugServerModule::HandleMessage(int connId, const Dia::WebSocket::Message& msg)
		{
			if (msg.type != Dia::WebSocket::MessageType::kText) return;

			mStats.messagesReceivedTotal++;

			dia::debug::DebugMessage protoMsg;
			if (!Dia::Proto::FromJson(msg.AsText(), &protoMsg))
			{
				DIA_LOG_ERROR("DebugServer", "DebugServerModule: HandleMessage connId=%d JSON parse error", connId);
				dia::debug::DebugMessage errorMsg;
				errorMsg.set_type(dia::debug::MESSAGE_TYPE_ERROR);
				errorMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
				auto* err = errorMsg.mutable_error();
				err->set_error_code("parse_error");
				err->set_message("Invalid JSON");
				SendProtoMessage(connId, errorMsg);
				return;
			}

			DIA_LOG_DEBUG("DebugServer",
				"DebugServerModule: HandleMessage connId=%d type=%d",
				connId, static_cast<int>(protoMsg.type()));

			switch (protoMsg.payload_case())
			{
			case dia::debug::DebugMessage::kSubscribe:
				HandleSubscribe(connId, protoMsg);
				break;
			case dia::debug::DebugMessage::kUnsubscribe:
				HandleUnsubscribe(connId, protoMsg);
				break;
			case dia::debug::DebugMessage::kCommandRequest:
				HandleCommand(connId, protoMsg);
				break;
			case dia::debug::DebugMessage::kHandshakeRequest:
				HandleHandshake(connId, protoMsg);
				break;
			case dia::debug::DebugMessage::kPing:
				HandlePing(connId, protoMsg);
				break;
			default:
			{
				dia::debug::DebugMessage errorMsg;
				errorMsg.set_type(dia::debug::MESSAGE_TYPE_ERROR);
				errorMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
				auto* err = errorMsg.mutable_error();
				err->set_error_code("unknown_type");
				err->set_message("Unknown message type");
				SendProtoMessage(connId, errorMsg);
				break;
			}
			}
		}

		void DebugServerModule::HandleSubscribe(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& sub = msg.subscribe();
			if (sub.data_type().empty())
			{
				dia::debug::DebugMessage errorMsg;
				errorMsg.set_type(dia::debug::MESSAGE_TYPE_ERROR);
				errorMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
				auto* err = errorMsg.mutable_error();
				err->set_error_code("invalid_subscribe");
				err->set_message("Missing data_type");
				SendProtoMessage(connId, errorMsg);
				return;
			}

			Dia::Core::StringCRC dataType(sub.data_type().c_str());
			Json::Value filter;
			if (sub.has_filter())
				filter = Dia::Proto::ProtoStructToJsonValue(sub.filter());
			mSubscriptionManager.Subscribe(connId, dataType, filter);
		}

		void DebugServerModule::HandleUnsubscribe(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& unsub = msg.unsubscribe();
			if (unsub.data_type().empty()) return;

			Dia::Core::StringCRC dataType(unsub.data_type().c_str());
			mSubscriptionManager.Unsubscribe(connId, dataType);
		}

		void DebugServerModule::HandleCommand(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& cmd = msg.command_request();
			if (cmd.command().empty())
			{
				dia::debug::DebugMessage errorMsg;
				errorMsg.set_type(dia::debug::MESSAGE_TYPE_ERROR);
				errorMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
				auto* err = errorMsg.mutable_error();
				err->set_error_code("invalid_command");
				err->set_message("Missing command field");
				SendProtoMessage(connId, errorMsg);
				return;
			}

			Dia::Core::StringCRC commandName(cmd.command().c_str());
			Json::Value payload;
			if (cmd.has_payload())
				payload = Dia::Proto::ProtoStructToJsonValue(cmd.payload());

			dia::debug::DebugMessage response;
			response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
			response.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* resp = response.mutable_command_response();
			resp->set_command(cmd.command());

			if (mQueryRegistry.Has(commandName))
			{
				DIA_LOG_DEBUG("DebugServer", "HandleCommand: '%s' dispatched via QueryRegistry",
					cmd.command().c_str());
				Json::Value result = mQueryRegistry.Execute(commandName, payload);
				resp->set_success(true);
				Dia::Proto::JsonValueToProtoStruct(result, resp->mutable_payload());
			}
			else
			{
				DIA_LOG_DEBUG("DebugServer", "HandleCommand: '%s' dispatched via DiaAPI fallback",
					cmd.command().c_str());
				Json::Value result = mCommandDispatcher.ExecuteDiaAPICommand(commandName, payload);
				resp->set_success(result.get("success", false).asBool());
				resp->set_message(result.get("message", "").asString());
			}

			SendProtoMessage(connId, response);
		}

		void DebugServerModule::HandleHandshake(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& request = msg.handshake_request();
			bool accepted = (request.protocol_version() == Dia::DebugProtocol::kProtocolVersion);

			dia::debug::DebugMessage response;
			response.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
			response.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* resp = response.mutable_handshake_response();
			resp->set_protocol_version(Dia::DebugProtocol::kProtocolVersion);
			resp->set_accepted(accepted);
			resp->set_server_name("DiaDebugServer");
			resp->set_server_version("2.0.0");
			SendProtoMessage(connId, response);
		}

		void DebugServerModule::HandlePing(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& ping = msg.ping();
			DIA_LOG_DEBUG("DebugServer",
				"DebugServerModule: HandlePing connId=%d ts=%llu, sending pong",
				connId, static_cast<unsigned long long>(ping.ts()));
			dia::debug::DebugMessage pongMsg;
			pongMsg.set_type(dia::debug::MESSAGE_TYPE_PONG);
			pongMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			pongMsg.mutable_pong()->set_ts(ping.ts());
			SendProtoMessage(connId, pongMsg);
		}

		//---------------------------------------------------------------------
		// Metrics & event broadcasts
		//---------------------------------------------------------------------

		void DebugServerModule::BroadcastCoreMetrics()
		{
			uint64_t serializeStart = Dia::DebugProtocol::GetTimestampNow();

			// --- Compute frame-time + FPS from the rolling accumulator.
			if (mFrameTimeSampleCount > 0)
			{
				mLastFrameTimeMsSample = mFrameTimeAccumMs / static_cast<float>(mFrameTimeSampleCount);
				mLastFpsSample = (mLastFrameTimeMsSample > 0.0f)
					? (1000.0f / mLastFrameTimeMsSample) : 0.0f;
			}
			mFrameTimeAccumMs = 0.0f;
			mFrameTimeSampleCount = 0;

			dia::debug::DebugMessage msg;
			msg.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
			msg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* cm = msg.mutable_core_metrics();

			cm->set_fps(mLastFpsSample);
			cm->set_frame_time_ms(mLastFrameTimeMsSample);
			cm->set_uptime_seconds(static_cast<float>(mStats.uptimeSeconds));

#ifdef _WIN32
			PROCESS_MEMORY_COUNTERS pmc;
			if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
			{
				cm->set_memory_used_mb(static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f));
			}

			MEMORYSTATUSEX memStatus;
			memStatus.dwLength = sizeof(memStatus);
			if (GlobalMemoryStatusEx(&memStatus))
			{
				cm->set_memory_available_mb(static_cast<float>(memStatus.ullAvailPhys) / (1024.0f * 1024.0f));
			}
#endif

			uint64_t serializeEnd = Dia::DebugProtocol::GetTimestampNow();
			mStats.serializationTimeMs = static_cast<float>(serializeEnd - serializeStart) / 1000.0f;

			uint64_t broadcastStart = Dia::DebugProtocol::GetTimestampNow();

			char jsonBuffer[4096];
			unsigned int jsonLength = 0;
			if (Dia::Proto::ToJson(msg, jsonBuffer, sizeof(jsonBuffer), &jsonLength))
				mServer->BroadcastText(jsonBuffer);

			uint64_t broadcastEnd = Dia::DebugProtocol::GetTimestampNow();
			mStats.broadcastTimeMs = static_cast<float>(broadcastEnd - broadcastStart) / 1000.0f;

			mStats.messagesSentTotal++;
			mStats.averageMessageSizeBytes = static_cast<int>(jsonLength);
			if (mMetricsBroadcastInterval > 0.0f)
			{
				mStats.bytesSentPerSec = static_cast<float>(jsonLength) / mMetricsBroadcastInterval;
			}
		}

		void DebugServerModule::BroadcastStageTransition(const Dia::Core::StringCRC& from,
		                                                  const Dia::Core::StringCRC& to)
		{
			auto subscribers = mSubscriptionManager.GetSubscribers(kStageTransitionTopic);
			if (subscribers.Size() == 0) return;

			Json::Value payload = StateSerializer::SerializeStageTransition(
				from, to, Dia::DebugProtocol::GetTimestampNow());

			dia::debug::DebugMessage msg;
			msg.set_type(dia::debug::MESSAGE_TYPE_EVENT);
			msg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* evt = msg.mutable_event();
			evt->set_event_type(kStageTransitionTopic.AsChar());
			Dia::Proto::JsonValueToProtoStruct(payload, evt->mutable_payload());

			char jsonBuffer[4096];
			if (Dia::Proto::ToJson(msg, jsonBuffer, sizeof(jsonBuffer)))
			{
				for (unsigned int i = 0; i < subscribers.Size(); ++i)
				{
					mServer->SendText(subscribers[i], jsonBuffer);
				}
			}
			mStats.messagesSentTotal += static_cast<int>(subscribers.Size());
		}

		void DebugServerModule::NotifySubscribers(const Dia::Core::StringCRC& dataType,
		                                           const Json::Value& payload)
		{
			auto subscribers = mSubscriptionManager.GetSubscribers(dataType);
			if (subscribers.Size() == 0) return;

			dia::debug::DebugMessage msg;
			msg.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
			msg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* update = msg.mutable_data_update();
			update->set_data_type(dataType.AsChar());
			Dia::Proto::JsonValueToProtoStruct(payload, update->mutable_payload());

			char jsonBuffer[4096];
			if (Dia::Proto::ToJson(msg, jsonBuffer, sizeof(jsonBuffer)))
			{
				for (unsigned int i = 0; i < subscribers.Size(); ++i)
				{
					mServer->SendText(subscribers[i], jsonBuffer);
				}
			}
			mStats.messagesSentTotal += static_cast<int>(subscribers.Size());
		}

		//---------------------------------------------------------------------
		// Query commands
		//---------------------------------------------------------------------

		void DebugServerModule::RegisterProtocolCommands()
		{
			mQueryRegistry.Register(
				Dia::Core::StringCRC("get_state"),
				[this](const Json::Value& /*args*/) -> Json::Value {
					auto* app = GetApplication();
					auto* insp = dynamic_cast<const Dia::ApplicationFlow::IApplicationInspectable*>(app);
					return StateSerializer::SerializeApplicationState(insp);
				}
			);

			mQueryRegistry.Register(
				Dia::Core::StringCRC("list_commands"),
				[this](const Json::Value& /*args*/) -> Json::Value {
					Json::Value result;

					Json::Value queryCmds(Json::arrayValue);
					queryCmds.append("get_state");
					queryCmds.append("list_commands");
					queryCmds.append("get_server_stats");
					result["query_commands"] = queryCmds;

					Json::Value apiCmds(Json::arrayValue);
					auto commands = Dia::API::ListCommands();
					for (unsigned int i = 0; i < commands.Size(); ++i)
					{
						apiCmds.append(commands[i]->name.AsChar());
					}
					result["api_commands"] = apiCmds;

					return result;
				}
			);

			mQueryRegistry.Register(
				Dia::Core::StringCRC("get_server_stats"),
				[this](const Json::Value& /*args*/) -> Json::Value {
					Json::Value result;

					result["game_side"]["debug_server_overhead_ms"] = mStats.debugServerOverheadMs;
					result["game_side"]["serialization_time_ms"]    = mStats.serializationTimeMs;
					result["game_side"]["broadcast_time_ms"]        = mStats.broadcastTimeMs;
					result["game_side"]["subscription_count"]       = mStats.subscriptionCount;

					result["editor_side"]["bytes_sent_per_sec"]        = mStats.bytesSentPerSec;
					result["editor_side"]["message_queue_size"]        = mStats.messageQueueSize;
					result["editor_side"]["messages_dropped"]          = mStats.messagesDropped;
					result["editor_side"]["average_message_size_bytes"]= mStats.averageMessageSizeBytes;

					result["server"]["connection_count"]        = mStats.connectionCount;
					result["server"]["messages_sent_total"]     = mStats.messagesSentTotal;
					result["server"]["messages_received_total"] = mStats.messagesReceivedTotal;
					result["server"]["uptime_seconds"]          = mStats.uptimeSeconds;

					return result;
				}
			);
		}

		//---------------------------------------------------------------------
		// Send helpers
		//---------------------------------------------------------------------

		void DebugServerModule::SendProtoMessage(int connId, const dia::debug::DebugMessage& msg)
		{
			char buffer[4096];
			if (Dia::Proto::ToJson(msg, buffer, sizeof(buffer)))
				mServer->SendText(connId, buffer);
			mStats.messagesSentTotal++;
		}

		void DebugServerModule::BroadcastProtoMessage(const dia::debug::DebugMessage& msg)
		{
			char buffer[4096];
			if (Dia::Proto::ToJson(msg, buffer, sizeof(buffer)))
				mServer->BroadcastText(buffer);
			mStats.messagesSentTotal++;
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

namespace { using DebugServerModule_ = Dia::DebugServer::DebugServerModule; }
DIA_MODULE(DebugServerModule_);
