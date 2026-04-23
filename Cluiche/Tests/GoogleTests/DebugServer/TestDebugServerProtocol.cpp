#include <gtest/gtest.h>

#include "DiaDebugProtocol/DiaDebugProtocol.h"
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

// ---------------------------------------------------------------------------
// Suite: DebugServerProtocol
//
// Tests the server-side proto message construction paths used by
// DebugServerModule and DebugServerLogSink, verifying that JSON produced
// by Dia::Proto::ToJson round-trips through Dia::Proto::FromJson with all
// fields intact.
// ---------------------------------------------------------------------------

// 1. HandshakeResponse_Construction
TEST(DebugServerProtocol, HandshakeResponse_Construction)
{
	dia::debug::DebugMessage welcome;
	welcome.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
	welcome.set_timestamp(12345);
	auto* resp = welcome.mutable_handshake_response();
	resp->set_protocol_version(1);
	resp->set_accepted(true);
	resp->set_server_name("DiaDebugServer");
	resp->set_server_version("1.0.0");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(welcome, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
	EXPECT_EQ(parsed.timestamp(), 12345u);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kHandshakeResponse);

	const auto& hr = parsed.handshake_response();
	EXPECT_EQ(hr.protocol_version(), 1);
	EXPECT_TRUE(hr.accepted());
	EXPECT_EQ(hr.server_name(), "DiaDebugServer");
	EXPECT_EQ(hr.server_version(), "1.0.0");
}

// 2. GameInfo_Construction
TEST(DebugServerProtocol, GameInfo_Construction)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	msg.set_timestamp(12345);
	auto* info = msg.mutable_game_info();
	info->set_name("TestGame");
	info->set_build("1.0.0");
	info->set_processing_unit_count(2);
	info->set_current_phase("UpdatePhase");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_GAME_INFO);
	EXPECT_EQ(parsed.timestamp(), 12345u);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kGameInfo);

	const auto& gi = parsed.game_info();
	EXPECT_EQ(gi.name(), "TestGame");
	EXPECT_EQ(gi.build(), "1.0.0");
	EXPECT_EQ(gi.processing_unit_count(), 2);
	EXPECT_EQ(gi.current_phase(), "UpdatePhase");
}

// 3. Pong_Construction
TEST(DebugServerProtocol, Pong_Construction)
{
	const uint64_t pingTs = 9876543210ULL;

	dia::debug::DebugMessage pongMsg;
	pongMsg.set_type(dia::debug::MESSAGE_TYPE_PONG);
	pongMsg.set_timestamp(12345);
	pongMsg.mutable_pong()->set_ts(pingTs);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(pongMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_PONG);
	EXPECT_EQ(parsed.timestamp(), 12345u);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kPong);
	EXPECT_EQ(parsed.pong().ts(), pingTs);
}

// 4. CoreMetrics_Construction
TEST(DebugServerProtocol, CoreMetrics_Construction)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
	msg.set_timestamp(12345);
	auto* cm = msg.mutable_core_metrics();
	cm->set_fps(60.0f);
	cm->set_frame_time_ms(16.6f);
	cm->set_memory_used_mb(512.5f);
	cm->set_memory_available_mb(15360.0f);
	cm->set_uptime_seconds(120.0f);

	auto* pu1 = cm->add_processing_units();
	pu1->set_name("MainPU");
	pu1->set_fps(60.0f);
	pu1->set_frame_time_ms(16.6f);

	auto* pu2 = cm->add_processing_units();
	pu2->set_name("RenderPU");
	pu2->set_fps(59.5f);
	pu2->set_frame_time_ms(16.8f);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_CORE_METRICS);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCoreMetrics);

	const auto& metrics = parsed.core_metrics();
	EXPECT_FLOAT_EQ(metrics.fps(), 60.0f);
	EXPECT_FLOAT_EQ(metrics.frame_time_ms(), 16.6f);
	EXPECT_FLOAT_EQ(metrics.memory_used_mb(), 512.5f);
	EXPECT_FLOAT_EQ(metrics.memory_available_mb(), 15360.0f);
	EXPECT_FLOAT_EQ(metrics.uptime_seconds(), 120.0f);

	ASSERT_EQ(metrics.processing_units_size(), 2);
	EXPECT_EQ(metrics.processing_units(0).name(), "MainPU");
	EXPECT_FLOAT_EQ(metrics.processing_units(0).fps(), 60.0f);
	EXPECT_FLOAT_EQ(metrics.processing_units(0).frame_time_ms(), 16.6f);
	EXPECT_EQ(metrics.processing_units(1).name(), "RenderPU");
	EXPECT_FLOAT_EQ(metrics.processing_units(1).fps(), 59.5f);
	EXPECT_FLOAT_EQ(metrics.processing_units(1).frame_time_ms(), 16.8f);
}

// 5. LogEntry_Construction
TEST(DebugServerProtocol, LogEntry_Construction)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_LOG);
	auto* log = msg.mutable_log();
	log->set_level("INFO");
	log->set_channel("DebugServer");
	log->set_message("Server started on port 8080");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_LOG);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kLog);

	const auto& entry = parsed.log();
	EXPECT_EQ(entry.level(), "INFO");
	EXPECT_EQ(entry.channel(), "DebugServer");
	EXPECT_EQ(entry.message(), "Server started on port 8080");
}

// 6. CommandResponse_Construction
TEST(DebugServerProtocol, CommandResponse_Construction)
{
	dia::debug::DebugMessage response;
	response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	response.set_timestamp(12345);
	auto* resp = response.mutable_command_response();
	resp->set_command("get_state");
	resp->set_success(true);
	resp->set_message("State retrieved");
	resp->set_payload_json("{\"processing_unit\":\"MainPU\"}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(response, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	EXPECT_EQ(parsed.timestamp(), 12345u);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCommandResponse);

	const auto& cr = parsed.command_response();
	EXPECT_EQ(cr.command(), "get_state");
	EXPECT_TRUE(cr.success());
	EXPECT_EQ(cr.message(), "State retrieved");
	EXPECT_EQ(cr.payload_json(), "{\"processing_unit\":\"MainPU\"}");
}

// 7. Error_Construction
TEST(DebugServerProtocol, Error_Construction)
{
	dia::debug::DebugMessage errorMsg;
	errorMsg.set_type(dia::debug::MESSAGE_TYPE_ERROR);
	errorMsg.set_timestamp(12345);
	auto* err = errorMsg.mutable_error();
	err->set_error_code("parse_error");
	err->set_message("Invalid JSON");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(errorMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_ERROR);
	EXPECT_EQ(parsed.timestamp(), 12345u);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kError);

	const auto& e = parsed.error();
	EXPECT_EQ(e.error_code(), "parse_error");
	EXPECT_EQ(e.message(), "Invalid JSON");
}

// 8. Event_Construction
TEST(DebugServerProtocol, Event_Construction)
{
	dia::debug::DebugMessage eventMsg;
	eventMsg.set_type(dia::debug::MESSAGE_TYPE_EVENT);
	eventMsg.set_timestamp(12345);
	auto* evt = eventMsg.mutable_event();
	evt->set_event_type("phase_transition");
	evt->set_payload_json("{\"pu_id\":\"MainPU\"}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(eventMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_EVENT);
	EXPECT_EQ(parsed.timestamp(), 12345u);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kEvent);

	const auto& e = parsed.event();
	EXPECT_EQ(e.event_type(), "phase_transition");
	EXPECT_EQ(e.payload_json(), "{\"pu_id\":\"MainPU\"}");
}

// 9. DataUpdate_Construction
TEST(DebugServerProtocol, DataUpdate_Construction)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
	msg.set_timestamp(12345);
	auto* update = msg.mutable_data_update();
	update->set_data_type("processing_unit_state");
	update->set_payload_json("{\"modules\":[\"RenderModule\",\"PhysicsModule\"]}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_DATA_UPDATE);
	EXPECT_EQ(parsed.timestamp(), 12345u);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kDataUpdate);

	const auto& du = parsed.data_update();
	EXPECT_EQ(du.data_type(), "processing_unit_state");
	EXPECT_EQ(du.payload_json(), "{\"modules\":[\"RenderModule\",\"PhysicsModule\"]}");
}

// 10. Subscribe_ParseFromClient
TEST(DebugServerProtocol, Subscribe_ParseFromClient)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE);
	msg.set_timestamp(12345);
	auto* sub = msg.mutable_subscribe();
	sub->set_data_type("phase_transition");
	sub->set_filter("{\"pu_id\":\"MainPU\"}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_SUBSCRIBE);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kSubscribe);
	EXPECT_EQ(parsed.subscribe().data_type(), "phase_transition");
	EXPECT_EQ(parsed.subscribe().filter(), "{\"pu_id\":\"MainPU\"}");
}

// 11. Unsubscribe_ParseFromClient
TEST(DebugServerProtocol, Unsubscribe_ParseFromClient)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_UNSUBSCRIBE);
	msg.set_timestamp(12345);
	auto* unsub = msg.mutable_unsubscribe();
	unsub->set_data_type("phase_transition");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_UNSUBSCRIBE);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kUnsubscribe);
	EXPECT_EQ(parsed.unsubscribe().data_type(), "phase_transition");
}

// 12. CommandRequest_ParseFromClient
TEST(DebugServerProtocol, CommandRequest_ParseFromClient)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
	msg.set_timestamp(12345);
	auto* cmd = msg.mutable_command_request();
	cmd->set_command("get_state");
	cmd->set_payload_json("{\"verbose\":true}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCommandRequest);
	EXPECT_EQ(parsed.command_request().command(), "get_state");
	EXPECT_EQ(parsed.command_request().payload_json(), "{\"verbose\":true}");
}

// 13. Ping_ParseFromClient
TEST(DebugServerProtocol, Ping_ParseFromClient)
{
	const uint64_t clientTs = 1234567890ULL;

	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_PING);
	msg.set_timestamp(12345);
	msg.mutable_ping()->set_ts(clientTs);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_PING);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kPing);
	EXPECT_EQ(parsed.ping().ts(), clientTs);
}

// 14. HandshakeRequest_ParseFromClient
TEST(DebugServerProtocol, HandshakeRequest_ParseFromClient)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_REQUEST);
	msg.set_timestamp(12345);
	auto* req = msg.mutable_handshake_request();
	req->set_protocol_version(Dia::DebugProtocol::kProtocolVersion);
	req->set_client_name("DiaEditor");
	req->set_client_version("2.0.0");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.type(), dia::debug::MESSAGE_TYPE_HANDSHAKE_REQUEST);
	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kHandshakeRequest);

	const auto& hr = parsed.handshake_request();
	EXPECT_EQ(hr.protocol_version(), Dia::DebugProtocol::kProtocolVersion);
	EXPECT_EQ(hr.client_name(), "DiaEditor");
	EXPECT_EQ(hr.client_version(), "2.0.0");
}

// 15. ServerResponse_JsonContainsType
TEST(DebugServerProtocol, ServerResponse_JsonContainsType)
{
	dia::debug::DebugMessage welcome;
	welcome.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
	welcome.set_timestamp(12345);
	auto* resp = welcome.mutable_handshake_response();
	resp->set_protocol_version(1);
	resp->set_accepted(true);
	resp->set_server_name("DiaDebugServer");
	resp->set_server_version("1.0.0");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(welcome, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	Json::Value root;
	Json::Reader reader;
	ASSERT_TRUE(reader.parse(json, root));

	ASSERT_TRUE(root.isMember("type"));
	EXPECT_EQ(root["type"].asString(), "MESSAGE_TYPE_HANDSHAKE_RESPONSE");
}

// 16. ServerResponse_JsonContainsTimestamp
TEST(DebugServerProtocol, ServerResponse_JsonContainsTimestamp)
{
	dia::debug::DebugMessage msg;
	msg.set_type(dia::debug::MESSAGE_TYPE_PONG);
	msg.set_timestamp(12345);
	msg.mutable_pong()->set_ts(99);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(msg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	Json::Value root;
	Json::Reader reader;
	ASSERT_TRUE(reader.parse(json, root));

	ASSERT_TRUE(root.isMember("timestamp"));
	// protobuf JSON represents uint64 as a string
	EXPECT_EQ(root["timestamp"].asString(), "12345");
}

// 17. LogEntry_BatchVsSingle_BothParse
TEST(DebugServerProtocol, LogEntry_BatchVsSingle_BothParse)
{
	// Single log entry
	dia::debug::DebugMessage singleMsg;
	singleMsg.set_type(dia::debug::MESSAGE_TYPE_LOG);
	singleMsg.set_timestamp(12345);
	auto* log = singleMsg.mutable_log();
	log->set_level("WARN");
	log->set_channel("Core");
	log->set_message("Low memory");

	char singleJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(singleMsg, singleJson, sizeof(singleJson)));
	ASSERT_GT(strlen(singleJson), 0u);

	dia::debug::DebugMessage parsedSingle;
	ASSERT_TRUE(Dia::Proto::FromJson(singleJson, &parsedSingle));
	EXPECT_EQ(parsedSingle.payload_case(), dia::debug::DebugMessage::kLog);

	// Log batch with one entry
	dia::debug::DebugMessage batchMsg;
	batchMsg.set_type(dia::debug::MESSAGE_TYPE_LOG_BATCH);
	batchMsg.set_timestamp(12345);
	auto* batch = batchMsg.mutable_log_batch();
	auto* entry = batch->add_entries();
	entry->set_level("WARN");
	entry->set_channel("Core");
	entry->set_message("Low memory");

	char batchJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(batchMsg, batchJson, sizeof(batchJson)));
	ASSERT_GT(strlen(batchJson), 0u);

	dia::debug::DebugMessage parsedBatch;
	ASSERT_TRUE(Dia::Proto::FromJson(batchJson, &parsedBatch));
	EXPECT_EQ(parsedBatch.payload_case(), dia::debug::DebugMessage::kLogBatch);

	// Verify payload cases are distinct
	EXPECT_NE(parsedSingle.payload_case(), parsedBatch.payload_case());

	// Verify batch entry content
	ASSERT_EQ(parsedBatch.log_batch().entries_size(), 1);
	EXPECT_EQ(parsedBatch.log_batch().entries(0).level(), "WARN");
	EXPECT_EQ(parsedBatch.log_batch().entries(0).channel(), "Core");
	EXPECT_EQ(parsedBatch.log_batch().entries(0).message(), "Low memory");
}

// 18. CommandResponse_EmptyPayloadJson
TEST(DebugServerProtocol, CommandResponse_EmptyPayloadJson)
{
	dia::debug::DebugMessage response;
	response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	response.set_timestamp(12345);
	auto* resp = response.mutable_command_response();
	resp->set_command("do_something");
	resp->set_success(true);
	resp->set_message("Done");
	resp->set_payload_json("");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(response, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCommandResponse);
	// In proto3 JSON, empty strings are omitted by default, so after
	// round-tripping the field should return the default empty string.
	EXPECT_TRUE(parsed.command_response().payload_json().empty());
}

// 19. CommandResponse_WithPayloadJson
TEST(DebugServerProtocol, CommandResponse_WithPayloadJson)
{
	const std::string payloadContent = "{\"modules\":[\"Render\",\"Physics\"],\"count\":2}";

	dia::debug::DebugMessage response;
	response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	response.set_timestamp(12345);
	auto* resp = response.mutable_command_response();
	resp->set_command("get_state");
	resp->set_success(true);
	resp->set_message("State retrieved");
	resp->set_payload_json(payloadContent);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(response, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage parsed;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &parsed));

	EXPECT_EQ(parsed.payload_case(), dia::debug::DebugMessage::kCommandResponse);
	EXPECT_EQ(parsed.command_response().command(), "get_state");
	EXPECT_TRUE(parsed.command_response().success());
	EXPECT_EQ(parsed.command_response().payload_json(), payloadContent);
}
