#include <gtest/gtest.h>

#include "DiaDebugProtocol/DiaDebugProtocol.h"

#include <cstring>

// ============================================================================
// Proto Message Round-Trip via JSON
// ============================================================================

TEST(DebugProtocolProto, GameInfoRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	auto* info = msg.mutable_game_info();
	info->set_name("TestGame");
	info->set_build("1.0.0");
	info->set_processing_unit_count(2);
	info->set_current_phase("Running");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_GAME_INFO);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kGameInfo);
	EXPECT_EQ(parsed.game_info().name(), "TestGame");
	EXPECT_EQ(parsed.game_info().build(), "1.0.0");
	EXPECT_EQ(parsed.game_info().processing_unit_count(), 2);
	EXPECT_EQ(parsed.game_info().current_phase(), "Running");
}

TEST(DebugProtocolProto, HandshakeRequestRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_REQUEST);
	auto* req = msg.mutable_handshake_request();
	req->set_protocol_version(1);
	req->set_client_name("TestEditor");
	req->set_client_version("2.0.0");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kHandshakeRequest);
	EXPECT_EQ(parsed.handshake_request().protocol_version(), 1);
	EXPECT_EQ(parsed.handshake_request().client_name(), "TestEditor");
	EXPECT_EQ(parsed.handshake_request().client_version(), "2.0.0");
}

TEST(DebugProtocolProto, HandshakeResponseRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
	auto* resp = msg.mutable_handshake_response();
	resp->set_protocol_version(1);
	resp->set_accepted(true);
	resp->set_server_name("DiaDebugServer");
	resp->set_server_version("1.0.0");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kHandshakeResponse);
	EXPECT_TRUE(parsed.handshake_response().accepted());
	EXPECT_EQ(parsed.handshake_response().server_name(), "DiaDebugServer");
}

TEST(DebugProtocolProto, PingPongRoundTrip)
{
	dia::debug::DebugMessage pingMsg;
	pingMsg.set_type(dia::debug::MESSAGE_TYPE_PING);
	pingMsg.mutable_ping()->set_ts(1234567890ULL);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(pingMsg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kPing);
	EXPECT_EQ(parsed.ping().ts(), 1234567890ULL);

	dia::debug::DebugMessage pongMsg;
	pongMsg.set_type(dia::debug::MESSAGE_TYPE_PONG);
	pongMsg.mutable_pong()->set_ts(parsed.ping().ts());

	ASSERT_TRUE(Dia::Proto::ToJson(pongMsg, json, sizeof(json)));
	dia::debug::DebugMessage parsedPong;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsedPong));
	EXPECT_EQ(parsedPong.payload_case(), dia::debug::DebugMessage::kPong);
	EXPECT_EQ(parsedPong.pong().ts(), 1234567890ULL);
}

TEST(DebugProtocolProto, CoreMetricsRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
	auto* cm = msg.mutable_core_metrics();
	cm->set_fps(60.0f);
	cm->set_frame_time_ms(16.67f);
	cm->set_memory_used_mb(512.0f);
	cm->set_memory_available_mb(2048.0f);
	cm->set_uptime_seconds(120.5f);

	auto* pu = cm->add_processing_units();
	pu->set_name("MainPU");
	pu->set_fps(60.0f);
	pu->set_frame_time_ms(16.67f);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCoreMetrics);
	EXPECT_FLOAT_EQ(parsed.core_metrics().fps(), 60.0f);
	EXPECT_FLOAT_EQ(parsed.core_metrics().frame_time_ms(), 16.67f);
	EXPECT_FLOAT_EQ(parsed.core_metrics().memory_used_mb(), 512.0f);
	EXPECT_FLOAT_EQ(parsed.core_metrics().memory_available_mb(), 2048.0f);
	EXPECT_EQ(parsed.core_metrics().processing_units_size(), 1);
	EXPECT_EQ(parsed.core_metrics().processing_units(0).name(), "MainPU");
}

TEST(DebugProtocolProto, CommandRequestRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
	auto* cmd = msg.mutable_command_request();
	cmd->set_command("hot_reload");
	cmd->set_payload_json("{\"path\":\"manifest.diaapp\"}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCommandRequest);
	EXPECT_EQ(parsed.command_request().command(), "hot_reload");
	EXPECT_EQ(parsed.command_request().payload_json(), "{\"path\":\"manifest.diaapp\"}");
}

TEST(DebugProtocolProto, CommandResponseRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	auto* resp = msg.mutable_command_response();
	resp->set_command("hot_reload");
	resp->set_success(true);
	resp->set_message("Reload completed");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCommandResponse);
	EXPECT_TRUE(parsed.command_response().success());
	EXPECT_EQ(parsed.command_response().message(), "Reload completed");
}

TEST(DebugProtocolProto, ErrorRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_ERROR);
	auto* err = msg.mutable_error();
	err->set_error_code("PARSE_ERROR");
	err->set_message("Invalid JSON");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kError);
	EXPECT_EQ(parsed.error().error_code(), "PARSE_ERROR");
	EXPECT_EQ(parsed.error().message(), "Invalid JSON");
}

TEST(DebugProtocolProto, LogEntryRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_LOG);
	auto* log = msg.mutable_log();
	log->set_level("warning");
	log->set_channel("Render");
	log->set_message("Frame drop detected");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kLog);
	EXPECT_EQ(parsed.log().level(), "warning");
	EXPECT_EQ(parsed.log().channel(), "Render");
	EXPECT_EQ(parsed.log().message(), "Frame drop detected");
}

TEST(DebugProtocolProto, LogBatchRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_LOG_BATCH);
	auto* batch = msg.mutable_log_batch();

	auto* e1 = batch->add_entries();
	e1->set_level("info");
	e1->set_channel("Core");
	e1->set_message("Started");

	auto* e2 = batch->add_entries();
	e2->set_level("error");
	e2->set_channel("Net");
	e2->set_message("Timeout");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kLogBatch);
	EXPECT_EQ(parsed.log_batch().entries_size(), 2);
	EXPECT_EQ(parsed.log_batch().entries(0).level(), "info");
	EXPECT_EQ(parsed.log_batch().entries(1).message(), "Timeout");
}

TEST(DebugProtocolProto, DataUpdateRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
	auto* update = msg.mutable_data_update();
	update->set_data_type("processing_unit_state");
	update->set_payload_json("{\"pu_id\":\"MainPU\"}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kDataUpdate);
	EXPECT_EQ(parsed.data_update().data_type(), "processing_unit_state");
}

TEST(DebugProtocolProto, EventRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_EVENT);
	auto* evt = msg.mutable_event();
	evt->set_event_type("phase_transition");
	evt->set_payload_json("{\"from\":\"Init\",\"to\":\"Update\"}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kEvent);
	EXPECT_EQ(parsed.event().event_type(), "phase_transition");
}

TEST(DebugProtocolProto, SubscribeRoundTrip)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE);
	auto* sub = msg.mutable_subscribe();
	sub->set_data_type("core_metrics");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	dia::debug::DebugMessage parsed;
	EXPECT_TRUE(Dia::Proto::FromJson(json, &parsed));
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kSubscribe);
	EXPECT_EQ(parsed.subscribe().data_type(), "core_metrics");
}

// ============================================================================
// Parse Failure / Edge Cases
// ============================================================================

TEST(DebugProtocolProto, FromJson_MalformedJsonReturnsFalse)
{
	dia::debug::DebugMessage msg;
	EXPECT_FALSE(Dia::Proto::FromJson("not json {{{", &msg));
}

TEST(DebugProtocolProto, FromJson_EmptyObjectSucceeds)
{
	dia::debug::DebugMessage msg;
	EXPECT_TRUE(Dia::Proto::FromJson("{}", &msg));
	EXPECT_EQ(msg.payload_case(), dia::debug::DebugMessage::PAYLOAD_NOT_SET);
}

TEST(DebugProtocolProto, FromJson_NullMessageReturnsFalse)
{
	EXPECT_FALSE(Dia::Proto::FromJson("{}", nullptr));
}

// ============================================================================
// Wire Compatibility: JSON field names match what JS UI expects
// ============================================================================

TEST(DebugProtocolProto, WireCompat_GameInfoFieldNames)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	auto* info = msg.mutable_game_info();
	info->set_name("Test");
	info->set_build("1.0");
	info->set_processing_unit_count(3);
	info->set_current_phase("Running");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	std::string jsonStr(json);
	EXPECT_NE(jsonStr.find("\"processing_unit_count\""), std::string::npos);
	EXPECT_NE(jsonStr.find("\"current_phase\""), std::string::npos);
	EXPECT_NE(jsonStr.find("\"name\""), std::string::npos);
	EXPECT_NE(jsonStr.find("\"build\""), std::string::npos);
}

TEST(DebugProtocolProto, WireCompat_CoreMetricsFieldNames)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
	auto* cm = msg.mutable_core_metrics();
	cm->set_fps(60.0f);
	cm->set_frame_time_ms(16.0f);
	cm->set_memory_used_mb(512.0f);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	std::string jsonStr(json);
	EXPECT_NE(jsonStr.find("\"fps\""), std::string::npos);
	EXPECT_NE(jsonStr.find("\"frame_time_ms\""), std::string::npos);
	EXPECT_NE(jsonStr.find("\"memory_used_mb\""), std::string::npos);
}

TEST(DebugProtocolProto, WireCompat_LogFieldNames)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_LOG);
	auto* log = msg.mutable_log();
	log->set_level("info");
	log->set_channel("Core");
	log->set_message("Test");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));

	std::string jsonStr(json);
	EXPECT_NE(jsonStr.find("\"level\""), std::string::npos);
	EXPECT_NE(jsonStr.find("\"channel\""), std::string::npos);
	EXPECT_NE(jsonStr.find("\"message\""), std::string::npos);
}

// ============================================================================
// Payload Case Exhaustiveness
// ============================================================================

TEST(DebugProtocolProto, PayloadCaseDistinct)
{
	dia::debug::DebugMessage msgs[12];

	msgs[0].mutable_handshake_request();
	msgs[1].mutable_handshake_response();
	msgs[2].mutable_subscribe();
	msgs[3].mutable_unsubscribe();
	msgs[4].mutable_core_metrics();
	msgs[5].mutable_data_update();
	msgs[6].mutable_event();
	msgs[7].mutable_command_request();
	msgs[8].mutable_command_response();
	msgs[9].mutable_error();
	msgs[10].mutable_ping();
	msgs[11].mutable_pong();

	for (int i = 0; i < 12; ++i)
	{
		for (int j = i + 1; j < 12; ++j)
		{
			EXPECT_NE(msgs[i].payload_case(), msgs[j].payload_case())
				<< "Payload case collision at indices " << i << " and " << j;
		}
	}
}
