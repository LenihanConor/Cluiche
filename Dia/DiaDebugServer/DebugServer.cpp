#include "DiaDebugServer/DebugServer.h"
#include "DiaDebugServer/StateSerializer.h"
#include "DiaDebugServer/IStreamTapTarget.h"

#include <DiaProtobuf/ProtoJsonCodec.h>
#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaWebSocket/Server.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cstring>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
// windows.h defines SetPort as a macro; drop it so we can call the real method.
#ifdef SetPort
#undef SetPort
#endif
#endif

namespace Dia
{
	namespace DebugServer
	{
		DebugServer::DebugServer()
			: mServer(nullptr)
			, mStateProvider(nullptr)
			, mPort(8080)
			, mAutoStart(true)
			, mObservationBridge(nullptr)
			, mSimSink(nullptr)
			, mNowMsFn(nullptr)
			, mSendHook(nullptr)
			, mSendHookContext(nullptr)
			, mLoggedLateSimReply(false)
			, mStartTimestamp(0)
			, mStarted(false)
		{
			mGameName[0]    = '\0';
			mGameBuild[0]   = '\0';
			mDiagamePath[0] = '\0';
			mQueryRegistry.SetRegisterVeto(&DebugServer::SimNameVeto, this);
		}

		bool DebugServer::SimNameVeto(void* context, const Dia::Core::StringCRC& name)
		{
			const DebugServer* self = static_cast<const DebugServer*>(context);
			for (unsigned int i = 0; i < self->mSimNames.Size(); ++i)
			{
				if (self->mSimNames[i].name == name)
					return true;
			}
			return false;
		}

		DebugServer::~DebugServer()
		{
			if (mStarted)
				Stop();
		}

		void DebugServer::SetGameInfo(const char* name, const char* build)
		{
			if (name) strncpy_s(mGameName, sizeof(mGameName), name, _TRUNCATE);
			else mGameName[0] = '\0';
			if (build) strncpy_s(mGameBuild, sizeof(mGameBuild), build, _TRUNCATE);
			else mGameBuild[0] = '\0';
		}

		void DebugServer::SetDiagamePath(const char* diagamePath)
		{
			if (diagamePath) strncpy_s(mDiagamePath, sizeof(mDiagamePath), diagamePath, _TRUNCATE);
			else mDiagamePath[0] = '\0';
		}

		//---------------------------------------------------------------------
		// Sim-hosted queries (Main thread only)
		//---------------------------------------------------------------------

		namespace
		{
			// Sim-hosted counters (dia.debugserver.sim.*). RegisterCounter is idempotent; call rate is per client command.
			void BumpSimMetric(const char* name)
			{
				if (auto* c = Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(Dia::Core::StringCRC(name)))
					c->Inc();
			}

			void SetSimPendingGauge(uint32_t count)
			{
				if (auto* g = Dia::Observation::Metric::MetricRegistry::Instance().RegisterGauge(
						Dia::Core::StringCRC("dia.debugserver.sim.pending")))
					g->Set(static_cast<double>(count));
			}
		}

		void DebugServer::SetSimRequestSink(ISimRequestSink* sink)
		{
			mSimSink = sink;
			if (sink)
				DIA_LOG_INFO("DebugServer", "Sim request sink attached");
			else
				DIA_LOG_INFO("DebugServer", "Sim request sink detached");
		}

		bool DebugServer::RegisterSimHosted(Dia::Core::StringCRC name, SimKind kind)
		{
			for (unsigned int i = 0; i < mSimNames.Size(); ++i)
			{
				if (mSimNames[i].name == name)
				{
					DIA_LOG_ERROR("DebugServer", "RegisterSimHosted: '%s' is already sim-hosted", name.AsChar());
					return false;
				}
			}
			if (mQueryRegistry.Has(name))
			{
				DIA_LOG_ERROR("DebugServer", "RegisterSimHosted: '%s' already exists in QueryRegistry", name.AsChar());
				return false;
			}
			if (mSimNames.IsFull())
			{
				DIA_LOG_ERROR("DebugServer", "RegisterSimHosted: capacity reached, cannot register '%s'", name.AsChar());
				return false;
			}
			SimName entry;
			entry.name = name;
			entry.kind = kind;
			mSimNames.Add(entry);
			DIA_LOG_INFO("DebugServer", "Sim-hosted name '%s' registered (kind=%u)", name.AsChar(), static_cast<unsigned>(kind));
			return true;
		}

		void DebugServer::UnregisterSimHosted(Dia::Core::StringCRC name)
		{
			for (unsigned int i = 0; i < mSimNames.Size(); ++i)
			{
				if (mSimNames[i].name == name)
				{
					mSimNames.RemoveAt(i);
					DIA_LOG_INFO("DebugServer", "Sim-hosted name '%s' unregistered", name.AsChar());
					return;
				}
			}
		}

		void DebugServer::SetNowMsFunction(NowMsFunction fn)
		{
			mNowMsFn = fn;
		}

		uint64_t DebugServer::NowMs() const
		{
			if (mNowMsFn)
				return mNowMsFn();
			return Dia::DebugProtocol::GetTimestampNow() / 1000;
		}

		void DebugServer::SendSimError(int connId, const char* commandName, const char* error)
		{
			dia::debug::DebugMessage response;
			response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
			response.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* resp = response.mutable_command_response();
			resp->set_command(commandName);
			resp->set_success(false);
			resp->set_message(error);
			Json::Value body;
			body["success"] = false;
			body["error"] = error;
			Dia::Proto::JsonValueToProtoStruct(body, resp->mutable_payload());
			SendProtoMessage(connId, response);
		}

		void DebugServer::SweepSimTimeouts()
		{
			if (mSimPending.Count() == 0)
				return;
			SimPendingTable::Entry expired[SimPendingTable::kCapacity];
			const uint32_t n = mSimPending.Expire(NowMs(), expired, SimPendingTable::kCapacity);
			for (uint32_t i = 0; i < n; ++i)
			{
				const char* name = expired[i].commandName;
				DIA_LOG_WARNING("DebugServer", "Sim-hosted request %u ('%s') timed out", expired[i].requestId, name);
				BumpSimMetric("dia.debugserver.sim.timeouts");
				SendSimError(expired[i].connectionId, name, "sim timeout");
			}
			SetSimPendingGauge(mSimPending.Count());
		}

		void DebugServer::CompleteSimReply(const SimReplyEvent& reply)
		{
			SimPendingTable::Entry entry;
			if (!mSimPending.Complete(reply.requestId, entry))
			{
				if (!mLoggedLateSimReply)
				{
					mLoggedLateSimReply = true;
					DIA_LOG_WARNING("DebugServer", "Dropping late/unknown sim reply (requestId=%u); further occurrences not logged",
						reply.requestId);
				}
				return;
			}

			SetSimPendingGauge(mSimPending.Count());

			const char* name = entry.commandName;

			switch (static_cast<SimReplyStatus>(reply.status))
			{
			case SimReplyStatus::Ok:
				break;
			case SimReplyStatus::TooLarge:
				BumpSimMetric("dia.debugserver.sim.too_large");
				SendSimError(entry.connectionId, name, "reply too large");
				return;
			case SimReplyStatus::UnknownName:
				SendSimError(entry.connectionId, name, "unknown name");
				return;
			case SimReplyStatus::HandlerFailed:
			default:
				BumpSimMetric("dia.debugserver.sim.handler_failed");
				SendSimError(entry.connectionId, name, "handler failed");
				return;
			}

			// Json must be NUL-terminated inside the fixed buffer.
			const void* nul = std::memchr(reply.json, 0, kSimPayloadBytes);
			Json::Value result;
			bool parsed = false;
			if (nul)
			{
				Json::CharReaderBuilder builder;
				std::string errs;
				std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
				parsed = reader->parse(reply.json, static_cast<const char*>(nul), &result, &errs);
			}
			if (!parsed)
			{
				DIA_LOG_ERROR("DebugServer", "Sim reply for '%s' has unparseable JSON", name);
				BumpSimMetric("dia.debugserver.sim.handler_failed");
				SendSimError(entry.connectionId, name, "handler failed");
				return;
			}

			dia::debug::DebugMessage response;
			response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
			response.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* resp = response.mutable_command_response();
			resp->set_command(name);
			resp->set_success(true);
			Dia::Proto::JsonValueToProtoStruct(result, resp->mutable_payload());
			SendProtoMessage(entry.connectionId, response);
			BumpSimMetric("dia.debugserver.sim.replies");
		}

		void DebugServer::SetStageTransitionCallback(StageTransitionCallback cb)
		{
			mStageTransitionCallback = std::move(cb);
		}

		void DebugServer::BroadcastStageTransition(Dia::Core::StringCRC fromStage, Dia::Core::StringCRC toStage)
		{
			if (!mServer) return;

			const Dia::Core::StringCRC stageTransitionTopic("stage_transition");
			Json::Value payload = StateSerializer::SerializeStageTransition(
				fromStage, toStage, Dia::DebugProtocol::GetTimestampNow());

			for (unsigned int i = 0; i < mClientTaps.Size(); ++i)
			{
				if (mClientTaps[i].streamId == stageTransitionTopic)
				{
					dia::debug::DebugMessage msg;
					msg.set_type(dia::debug::MESSAGE_TYPE_EVENT);
					msg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
					auto* e = msg.mutable_event();
					e->set_event_type(stageTransitionTopic.AsChar());
					Dia::Proto::JsonValueToProtoStruct(payload, e->mutable_payload());

					char jsonBuffer[4096];
					if (Dia::Proto::ToJson(msg, jsonBuffer, sizeof(jsonBuffer)))
					{
						if (mServer->SendText(mClientTaps[i].connId, jsonBuffer))
						{
							mStats.messagesSentTotal++;
							RecordTopicSent(stageTransitionTopic, true);
						}
						else
						{
							DIA_LOG_ERROR("DebugServer", "BroadcastStageTransition — queue full for connId=%d, message dropped",
								mClientTaps[i].connId);
							mStats.messagesDropped++;
							RecordTopicSent(stageTransitionTopic, false);
							SendDropNotification(mClientTaps[i].connId);
						}
					}
				}
			}
		}

		//---------------------------------------------------------------------
		// Lifecycle
		//---------------------------------------------------------------------

		void DebugServer::Start()
		{
			DIA_LOG_INFO("DebugServer", "DebugServer::Start port=%d autoStart=%d",
				static_cast<int>(mPort), mAutoStart ? 1 : 0);
			mServer = new Dia::WebSocket::Server(mPort);

			mServer->SetConnectionCallback([this](int connId, bool connected) {
				HandleConnection(connId, connected);
			});

			mServer->SetMessageCallback([this](int connId, const Dia::WebSocket::Message& msg) {
				HandleMessage(connId, msg);
			});

			RegisterProtocolCommands();

			if (mAutoStart)
				StartServer();

			mLogSink.SetServer(mServer);
			Dia::Observation::Log::Logger::Instance().RegisterSink(&mLogSink);

			mStartTimestamp = Dia::DebugProtocol::GetTimestampNow();
			mStarted = true;
		}

		void DebugServer::StartObservationBridge(const char* sessionId, int64_t epochOffsetNs)
		{
			if (!mServer || mObservationBridge)
				return;

			mObservationBridge = new ObservationBridge(mServer);

			// Wire subscriber query — provides gated send list per observation topic.
			// NOTE: The lambda captures `this` and reads mClientTaps which is mutated from
			// the tick thread; observation sinks fire from their own threads. For v1 we
			// accept this race (same as the pre-existing broadcast architecture).
			mObservationBridge->SetSubscriberQuery(
				[this](const char* topic, Dia::Core::Containers::DynamicArrayC<int, 16>& out)
				{
					Dia::Core::StringCRC topicCrc(topic);
					for (unsigned int i = 0; i < mClientTaps.Size(); ++i)
					{
						if (mClientTaps[i].streamId == topicCrc)
							out.Add(mClientTaps[i].connId);
					}
				}
			);

			mObservationBridge->Start(sessionId, epochOffsetNs);
		}

		void DebugServer::Tick(float deltaTime)
		{
			if (!mServer || !mServer->IsRunning()) return;

			uint64_t frameStart = Dia::DebugProtocol::GetTimestampNow();

			mLogSink.FlushToServer();
			mServer->Update();
			SweepSimTimeouts();

			DrainPublishedTopics();

			if (mObservationBridge)
				mObservationBridge->Flush(deltaTime);

			mStats.connectionCount   = mServer->GetConnectionCount();
			mStats.subscriptionCount = static_cast<int>(mClientTaps.Size());

			if (mStartTimestamp > 0)
			{
				uint64_t now = Dia::DebugProtocol::GetTimestampNow();
				mStats.uptimeSeconds = static_cast<int>((now - mStartTimestamp) / 1000000);
			}

			uint64_t frameEnd = Dia::DebugProtocol::GetTimestampNow();
			mStats.debugServerOverheadMs = static_cast<float>(frameEnd - frameStart) / 1000.0f;
		}

		void DebugServer::Stop()
		{
			if (!mStarted) return;

			// Detach all client taps
			for (unsigned int i = 0; i < mClientTaps.Size(); ++i)
			{
				if (mStateProvider)
				{
					auto* store = mStateProvider->FindStream(mClientTaps[i].streamId);
					if (store) store->DetachTap(Dia::DebugServer::TapHandle{mClientTaps[i].tapId});
				}
			}
			mClientTaps.RemoveAll();
			RebuildSubscriberCounts();

			if (mObservationBridge)
			{
				mObservationBridge->Stop();
				delete mObservationBridge;
				mObservationBridge = nullptr;
			}

			Dia::Observation::Log::Logger::Instance().UnregisterSink(&mLogSink);
			mLogSink.SetServer(nullptr);

			DIA_LOG_INFO("DebugServer", "DebugServer::Stop");
			if (mServer)
			{
				mServer->Stop();
				delete mServer;
				mServer = nullptr;
			}
			mStarted = false;
		}

		//---------------------------------------------------------------------
		// Server control
		//---------------------------------------------------------------------

		bool DebugServer::StartServer()
		{
			if (!mServer) return false;
			if (mServer->IsRunning()) return true;
			bool ok = mServer->Start();
			DIA_LOG_INFO("DebugServer", "DebugServer::StartServer on port %d result=%d",
				static_cast<int>(mPort), ok ? 1 : 0);
			return ok;
		}

		void DebugServer::StopServer()
		{
			if (mServer) mServer->Stop();
		}

		bool DebugServer::IsServerRunning() const
		{
			return mServer && mServer->IsRunning();
		}

		int DebugServer::GetConnectionCount() const
		{
			if (!mServer) return 0;
			return mServer->GetConnectionCount();
		}

		//---------------------------------------------------------------------
		// Connection / message handling
		//---------------------------------------------------------------------

		void DebugServer::HandleConnection(int connId, bool connected)
		{
			DIA_LOG_INFO("DebugServer",
				"DebugServer::HandleConnection connId=%d connected=%d totalConnections=%d",
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

				DIA_LOG_INFO("DebugServer", "DebugServer: Sending game_info to connId=%d", connId);
				SendGameInfo(connId);
			}
			else
			{
				DIA_LOG_INFO("DebugServer", "DebugServer: Client disconnected connId=%d", connId);
				// Detach all taps for this connection
				for (unsigned int i = 0; i < mClientTaps.Size(); )
				{
					if (mClientTaps[i].connId == connId)
					{
						if (mStateProvider)
						{
							auto* store = mStateProvider->FindStream(mClientTaps[i].streamId);
							if (store) store->DetachTap(Dia::DebugServer::TapHandle{mClientTaps[i].tapId});
						}
						mClientTaps.RemoveAt(i);
					}
					else
					{
						++i;
					}
				}
				RebuildSubscriberCounts();
				mSimPending.DropConnection(connId);
			}
		}

		void DebugServer::SendGameInfo(int connId)
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

		const char* DebugServer::GetCurrentStageName() const
		{
			if (!mStateProvider) return "";
			return mStateProvider->GetCurrentStage().AsChar();
		}

		int DebugServer::GetProcessingUnitCount() const
		{
			if (!mStateProvider) return 0;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> puIds;
			mStateProvider->GetProcessingUnitIds(puIds);
			return static_cast<int>(puIds.Size());
		}

		void DebugServer::HandleMessage(int connId, const Dia::WebSocket::Message& msg)
		{
			if (msg.type != Dia::WebSocket::MessageType::kText) return;

			mStats.messagesReceivedTotal++;

			dia::debug::DebugMessage protoMsg;
			if (!Dia::Proto::FromJson(msg.AsText(), &protoMsg))
			{
				DIA_LOG_ERROR("DebugServer", "DebugServer::HandleMessage connId=%d JSON parse error", connId);
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
				"DebugServer::HandleMessage connId=%d type=%d",
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

		void DebugServer::HandleSubscribe(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& sub = msg.subscribe();
			DIA_LOG_INFO("DebugServer", "HandleSubscribe: connId=%d dataType='%s'",
				connId, sub.data_type().c_str());
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
			const char* ackMessage = "subscribed";
			bool replayVirtualTopic = false;
			if (mStateProvider)
			{
				auto* store = mStateProvider->FindStream(dataType);
				if (store)
				{
					auto cb = [this, connId, dataType](const void* bytes, unsigned int size,
					                                    const Dia::Core::StringCRC& /*streamId*/)
					{
						Json::Value payload = mStateProvider->SerializeStreamPayload(
							dataType, bytes, static_cast<size_t>(size));

						dia::debug::DebugMessage updateMsg;
						updateMsg.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
						updateMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
						auto* update = updateMsg.mutable_data_update();
						update->set_data_type(dataType.AsChar());
						if (!payload.isNull())
							Dia::Proto::JsonValueToProtoStruct(payload, update->mutable_payload());

						char jsonBuffer[4096];
						if (mServer && Dia::Proto::ToJson(updateMsg, jsonBuffer, sizeof(jsonBuffer)))
						{
							if (mServer->SendText(connId, jsonBuffer))
							{
								RecordTopicSent(dataType, true);
							}
							else
							{
								DIA_LOG_ERROR("DebugServer", "HandleSubscribe tap — queue full for connId=%d topic='%s', message dropped",
									connId, dataType.AsChar());
								mStats.messagesDropped++;
								RecordTopicSent(dataType, false);
								SendDropNotification(connId);
							}
						}
						mStats.messagesSentTotal++;
					};

					auto tapHandle = store->AttachTap(cb);
					if (tapHandle.id != 0)
					{
						ClientTap ct;
						ct.connId   = connId;
						ct.streamId = dataType;
						ct.tapId    = tapHandle.id;
						mClientTaps.Add(ct);
						RebuildSubscriberCounts();
					}
				}
				else
				{
					// Virtual topic: no Application stream exists. Register in mClientTaps
					// with tapId == 0 so NotifySubscribers and the $lifecycle tap can still
					// push to subscribed clients by scanning streamId.
					ClientTap ct;
					ct.connId   = connId;
					ct.streamId = dataType;
					ct.tapId    = 0;
					mClientTaps.Add(ct);
					RebuildSubscriberCounts();
					replayVirtualTopic = true;
				}
			}
			else
			{
				ackMessage = "no_state_provider_queued";
			}

			// Send SUBSCRIBE_ACK back to the client
			dia::debug::DebugMessage ackMsg;
			ackMsg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE_ACK);
			ackMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* ack = ackMsg.mutable_subscribe_ack();
			ack->set_data_type(sub.data_type());
			ack->set_success(true);
			ack->set_message(ackMessage);
			SendProtoMessage(connId, ackMsg);

			// Late subscriber: send the last published value of a virtual topic (this connection only).
			if (replayVirtualTopic)
			{
				Json::Value retained;
				bool hasRetained = false;
				{
					Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mPublishMutex);
					for (unsigned int i = 0; i < mPublished.Size(); ++i)
					{
						if (mPublished[i].topic == dataType)
						{
							retained = mPublished[i].latest;
							hasRetained = true;
							break;
						}
					}
				}
				if (hasRetained)
				{
					DIA_LOG_DEBUG("DebugServer", "HandleSubscribe: replaying retained value of '%s' to connId=%d",
						dataType.AsChar(), connId);
					SendDataUpdateToConnection(connId, dataType, retained);
				}
			}
		}

		void DebugServer::HandleUnsubscribe(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& unsub = msg.unsubscribe();
			if (unsub.data_type().empty()) return;

			Dia::Core::StringCRC dataType(unsub.data_type().c_str());
			for (unsigned int i = 0; i < mClientTaps.Size(); )
			{
				if (mClientTaps[i].connId == connId && mClientTaps[i].streamId == dataType)
				{
					if (mStateProvider)
					{
						auto* store = mStateProvider->FindStream(dataType);
						if (store) store->DetachTap(Dia::DebugServer::TapHandle{mClientTaps[i].tapId});
					}
					mClientTaps.RemoveAt(i);
				}
				else
				{
					++i;
				}
			}
			RebuildSubscriberCounts();
		}

		void DebugServer::HandleCommand(int connId, const dia::debug::DebugMessage& msg)
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

			// Sim-hosted names are deferred: post to SimPU, reply in CompleteSimReply.
			for (unsigned int i = 0; i < mSimNames.Size(); ++i)
			{
				if (!(mSimNames[i].name == commandName))
					continue;

				std::string argsText = "{}";
				if (!payload.isNull())
				{
					Json::StreamWriterBuilder compact;
					compact["indentation"] = "";
					argsText = Json::writeString(compact, payload);
				}
				if (argsText.size() >= kSimPayloadBytes)
				{
					BumpSimMetric("dia.debugserver.sim.too_large");
					SendSimError(connId, cmd.command().c_str(), "reply too large");
					return;
				}

				const uint32_t requestId = mSimPending.Add(connId, commandName.Value(), NowMs(), cmd.command().c_str());
				if (requestId == 0 || !mSimSink)
				{
					if (requestId != 0)
					{
						SimPendingTable::Entry discard;
						mSimPending.Complete(requestId, discard);
					}
					DIA_LOG_WARNING("DebugServer", "HandleCommand: sim busy for '%s'", cmd.command().c_str());
					BumpSimMetric("dia.debugserver.sim.busy");
					SendSimError(connId, cmd.command().c_str(), "sim busy");
					return;
				}

				SimRequestEvent request;
				request.requestId = requestId;
				request.nameCrc = commandName.Value();
				request.kind = static_cast<uint32_t>(mSimNames[i].kind);
				std::memcpy(request.json, argsText.c_str(), argsText.size() + 1);
				if (!mSimSink->PostRequest(request))
				{
					SimPendingTable::Entry discard;
					mSimPending.Complete(requestId, discard);
					DIA_LOG_WARNING("DebugServer", "HandleCommand: sim request stream full for '%s'", cmd.command().c_str());
					BumpSimMetric("dia.debugserver.sim.busy");
					SendSimError(connId, cmd.command().c_str(), "sim busy");
				}
				else
				{
					BumpSimMetric("dia.debugserver.sim.requests");
					SetSimPendingGauge(mSimPending.Count());
				}
				return;
			}

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
				if (result.isMember("data") && result["data"].isObject())
					Dia::Proto::JsonValueToProtoStruct(result["data"], resp->mutable_payload());
				else if (result.isMember("error") && result["error"].isString())
					resp->set_message(result["error"].asString());
				else if (result.isMember("error") && result["error"].isObject())
				{
					const Json::Value& errorObj = result["error"];
					if (errorObj.isMember("message") && errorObj["message"].isString())
					{
						resp->set_message(errorObj["message"].asString());
					}
					else
					{
						Json::StreamWriterBuilder writer;
						writer["indentation"] = "";
						resp->set_message(Json::writeString(writer, errorObj));
					}
					Dia::Proto::JsonValueToProtoStruct(errorObj, resp->mutable_payload());
				}
			}

			SendProtoMessage(connId, response);
		}

		void DebugServer::HandleHandshake(int connId, const dia::debug::DebugMessage& msg)
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

		void DebugServer::HandlePing(int connId, const dia::debug::DebugMessage& msg)
		{
			const auto& ping = msg.ping();
			DIA_LOG_DEBUG("DebugServer",
				"DebugServer::HandlePing connId=%d ts=%llu, sending pong",
				connId, static_cast<unsigned long long>(ping.ts()));
			dia::debug::DebugMessage pongMsg;
			pongMsg.set_type(dia::debug::MESSAGE_TYPE_PONG);
			pongMsg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			pongMsg.mutable_pong()->set_ts(ping.ts());
			SendProtoMessage(connId, pongMsg);
		}

		//---------------------------------------------------------------------
		// Event broadcasts
		//---------------------------------------------------------------------

		void DebugServer::NotifySubscribers(const Dia::Core::StringCRC& dataType,
		                                     const Json::Value& payload)
		{
			NotifySubscribersMainThread(dataType, payload);
		}

		void DebugServer::NotifySubscribersMainThread(const Dia::Core::StringCRC& dataType,
		                                               const Json::Value& payload)
		{
			if (!mServer) return;

			unsigned int matchCount = 0;
			for (unsigned int i = 0; i < mClientTaps.Size(); ++i)
			{
				if (mClientTaps[i].streamId == dataType)
					++matchCount;
			}
			DIA_LOG_DEBUG("DebugServer", "NotifySubscribers: dataType='%s' totalTaps=%u matching=%u payloadNull=%d",
				dataType.AsChar(), mClientTaps.Size(), matchCount, payload.isNull() ? 1 : 0);

			if (matchCount == 0) return;

			dia::debug::DebugMessage msg;
			msg.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
			msg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* update = msg.mutable_data_update();
			update->set_data_type(dataType.AsChar());
			Dia::Proto::JsonValueToProtoStruct(payload, update->mutable_payload());

			char jsonBuffer[65536];
			if (!Dia::Proto::ToJson(msg, jsonBuffer, sizeof(jsonBuffer)))
			{
				DIA_LOG_WARNING("DebugServer", "NotifySubscribers: ToJson failed for dataType='%s' — payload too large for buffer",
					dataType.AsChar());
				return;
			}

			for (unsigned int i = 0; i < mClientTaps.Size(); ++i)
			{
				if (mClientTaps[i].streamId == dataType)
				{
					if (!mServer->SendText(mClientTaps[i].connId, jsonBuffer))
					{
						DIA_LOG_ERROR("DebugServer", "NotifySubscribers — queue full for connId=%d topic='%s', message dropped",
							mClientTaps[i].connId, dataType.AsChar());
						mStats.messagesDropped++;
						RecordTopicSent(dataType, false);
						SendDropNotification(mClientTaps[i].connId);
					}
					else
					{
						mStats.messagesSentTotal++;
						RecordTopicSent(dataType, true);
					}
				}
			}
		}

		unsigned int DebugServer::GetTopicSubscriberCount(const Dia::Core::StringCRC& topic) const
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mSubscriberMutex);
			for (unsigned int i = 0; i < mSubscriberCounts.Size(); ++i)
			{
				if (mSubscriberCounts[i].topic == topic) return mSubscriberCounts[i].count;
			}
			return 0;
		}

		// Recomputed from mClientTaps (the single source of truth) after every mutation, so the counts cannot drift.
		// Counts distinct connections per topic: duplicate subscribes from one connection count once (unsubscribe removes all).
		void DebugServer::RebuildSubscriberCounts()
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mSubscriberMutex);
			mSubscriberCounts.RemoveAll();
			for (unsigned int i = 0; i < mClientTaps.Size(); ++i)
			{
				bool duplicate = false;
				for (unsigned int j = 0; j < i; ++j)
				{
					if (mClientTaps[j].connId == mClientTaps[i].connId && mClientTaps[j].streamId == mClientTaps[i].streamId)
					{
						duplicate = true;
						break;
					}
				}
				if (duplicate) continue;

				bool found = false;
				for (unsigned int k = 0; k < mSubscriberCounts.Size(); ++k)
				{
					if (mSubscriberCounts[k].topic == mClientTaps[i].streamId)
					{
						++mSubscriberCounts[k].count;
						found = true;
						break;
					}
				}
				if (!found && !mSubscriberCounts.IsFull())
				{
					TopicSubscribers ts;
					ts.topic = mClientTaps[i].streamId;
					ts.count = 1;
					mSubscriberCounts.Add(ts);
				}
			}
		}

		void DebugServer::PublishTopic(const Dia::Core::StringCRC& topic, const Json::Value& payload)
		{
			Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mPublishMutex);
			for (unsigned int i = 0; i < mPublished.Size(); ++i)
			{
				if (mPublished[i].topic == topic)
				{
					mPublished[i].latest  = payload;
					mPublished[i].pending = true;
					return;
				}
			}
			if (mPublished.IsFull())
			{
				DIA_LOG_ERROR("DebugServer", "PublishTopic — topic table full (%u), topic '%s' dropped",
					kMaxPublishedTopics, topic.AsChar());
				return;
			}
			PublishedTopic entry;
			entry.topic   = topic;
			entry.latest  = payload;
			entry.pending = true;
			mPublished.Add(entry);
			DIA_LOG_DEBUG("DebugServer", "PublishTopic: new topic '%s'", topic.AsChar());
		}

		void DebugServer::DrainPublishedTopics()
		{
			// Copy pending payloads out under the lock; send outside it.
			Dia::Core::Containers::DynamicArrayC<PublishedTopic, kMaxPublishedTopics> toSend;
			{
				Dia::Core::ScopedLock<Dia::Core::Mutex> lock(mPublishMutex);
				for (unsigned int i = 0; i < mPublished.Size(); ++i)
				{
					if (mPublished[i].pending)
					{
						toSend.Add(mPublished[i]);
						mPublished[i].pending = false;
					}
				}
			}
			for (unsigned int i = 0; i < toSend.Size(); ++i)
				NotifySubscribersMainThread(toSend[i].topic, toSend[i].latest);
		}

		void DebugServer::SendDataUpdateToConnection(int connId, const Dia::Core::StringCRC& topic,
		                                              const Json::Value& payload)
		{
			if (!mServer) return;

			dia::debug::DebugMessage msg;
			msg.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
			msg.set_timestamp(Dia::DebugProtocol::GetTimestampNow());
			auto* update = msg.mutable_data_update();
			update->set_data_type(topic.AsChar());
			Dia::Proto::JsonValueToProtoStruct(payload, update->mutable_payload());

			char jsonBuffer[65536];
			if (!Dia::Proto::ToJson(msg, jsonBuffer, sizeof(jsonBuffer)))
			{
				DIA_LOG_WARNING("DebugServer", "SendDataUpdateToConnection: ToJson failed for topic='%s' — payload too large for buffer",
					topic.AsChar());
				return;
			}
			if (mServer->SendText(connId, jsonBuffer))
			{
				mStats.messagesSentTotal++;
				RecordTopicSent(topic, true);
			}
			else
			{
				DIA_LOG_ERROR("DebugServer", "SendDataUpdateToConnection — queue full for connId=%d topic='%s', message dropped",
					connId, topic.AsChar());
				mStats.messagesDropped++;
				RecordTopicSent(topic, false);
				SendDropNotification(connId);
			}
		}

		//---------------------------------------------------------------------
		// Query commands
		//---------------------------------------------------------------------

		void DebugServer::RegisterProtocolCommands()
		{
			mQueryRegistry.Register(
				Dia::Core::StringCRC("get_app_state"),
				[this](const Json::Value& /*args*/) -> Json::Value {
					Json::Value result;
					result["diagame_path"] = mDiagamePath;
					return result;
				}
			);

			mQueryRegistry.Register(
				Dia::Core::StringCRC("get_state"),
				[this](const Json::Value& /*args*/) -> Json::Value {
					return StateSerializer::SerializeApplicationState(mStateProvider);
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
					Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfo, 256> commands;
					Dia::API::ListCommandInfos(commands);
					for (unsigned int i = 0; i < commands.Size(); ++i)
						apiCmds.append(commands[i].name.AsChar());
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

					// Compute drop rates and serialize topics
					Json::Value topicsArray(Json::arrayValue);
					for (int i = 0; i < mStats.topicStatCount; ++i)
					{
						TopicStats& ts = mStats.topicStats[i];
						int total = ts.messagesSent + ts.messagesDropped;
						ts.dropRatePercent = (total > 0)
							? static_cast<float>(ts.messagesDropped) * 100.0f / static_cast<float>(total)
							: 0.0f;

						Json::Value entry;
						entry["topic"]     = ts.topic.AsChar() ? ts.topic.AsChar() : "";
						entry["sent"]      = ts.messagesSent;
						entry["dropped"]   = ts.messagesDropped;
						entry["drop_rate"] = ts.dropRatePercent;
						topicsArray.append(entry);
					}
					result["topics"] = topicsArray;

					return result;
				}
			);
		}

		//---------------------------------------------------------------------
		// Send helpers
		//---------------------------------------------------------------------

		void DebugServer::SendProtoMessage(int connId, const dia::debug::DebugMessage& msg)
		{
			if (mSendHook)
			{
				mSendHook(mSendHookContext, connId, msg);
				return;
			}
			std::string buffer;
			if (Dia::Proto::ToJson(msg, buffer))
			{
				if (!mServer->SendText(connId, buffer.c_str()))
				{
					DIA_LOG_ERROR("DebugServer", "DebugServer::SendProtoMessage — queue full for connId=%d, message dropped", connId);
					mStats.messagesDropped++;
					SendDropNotification(connId);
				}
				else
				{
					mStats.messagesSentTotal++;
				}
			}
		}

		void DebugServer::BroadcastProtoMessage(const dia::debug::DebugMessage& msg)
		{
			std::string buffer;
			if (!Dia::Proto::ToJson(msg, buffer))
				return;

			if (!mServer) return;
			int connCount = mServer->GetConnectionCount();
			mServer->BroadcastText(buffer.c_str());
			mStats.messagesSentTotal += connCount;
		}

		void DebugServer::SendJsonToConnection(int connId, const Json::Value& json)
		{
			std::string str = Json::FastWriter().write(json);
			if (mServer->SendText(connId, str.c_str()))
			{
				mStats.messagesSentTotal++;
			}
			else
			{
				DIA_LOG_ERROR("DebugServer", "SendJsonToConnection — queue full for connId=%d, message dropped", connId);
				mStats.messagesDropped++;
				SendDropNotification(connId);
			}
		}

		void DebugServer::BroadcastJson(const Json::Value& json)
		{
			std::string str = Json::FastWriter().write(json);
			if (!mServer) return;
			int connCount = mServer->GetConnectionCount();
			mServer->BroadcastText(str.c_str());
			mStats.messagesSentTotal += connCount;
		}

		void DebugServer::RecordTopicSent(const Dia::Core::StringCRC& topic, bool success)
		{
			// Find existing entry
			for (int i = 0; i < mStats.topicStatCount; ++i)
			{
				if (mStats.topicStats[i].topic == topic)
				{
					if (success) mStats.topicStats[i].messagesSent++;
					else         mStats.topicStats[i].messagesDropped++;
					return;
				}
			}
			// New topic
			if (mStats.topicStatCount < ServerStats::kMaxTrackedTopics)
			{
				TopicStats& ts = mStats.topicStats[mStats.topicStatCount++];
				ts.topic = topic;
				if (success) ts.messagesSent = 1;
				else         ts.messagesDropped = 1;
			}
			else
			{
				DIA_LOG_WARNING("DebugServer", "RecordTopicSent — topic tracking full (%d slots), topic '%s' stats will not be tracked",
					ServerStats::kMaxTrackedTopics, topic.AsChar());
			}
		}

		void DebugServer::SendDropNotification(int connId)
		{
			if (!mServer) return;

			// Rate-limit: max 1 notification per 5 seconds per connection
			uint64_t nowUs = Dia::DebugProtocol::GetTimestampNow();
			static const uint64_t kRateLimitUs = 5000000ULL; // 5 seconds in microseconds
			if (nowUs - mStats.lastDropNotifyTimestampUs < kRateLimitUs)
				return;

			mStats.lastDropNotifyTimestampUs = nowUs;

			// Send as plain JSON (not proto) since it's a topic-based notification
			char notifyBuf[256];
			snprintf(notifyBuf, sizeof(notifyBuf),
				"{\"topic\":\"editor.notification\","
				"\"level\":\"error\","
				"\"title\":\"Connection degraded\","
				"\"message\":\"Messages being dropped\"}");

			// Try to send — if this also fails we don't recurse (rate limit already updated)
			mServer->SendText(connId, notifyBuf);
		}
	}
}
