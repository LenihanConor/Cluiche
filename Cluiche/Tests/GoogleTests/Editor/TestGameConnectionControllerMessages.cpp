#include <gtest/gtest.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaDebugProtocol/DiaDebugProtocol.h>
#include <DiaCore/Json/external/json/json.h>

#include <string>
#include <cstring>

using namespace Dia::Editor;

// ---------------------------------------------------------------------------
// These tests exercise the proto-JSON round-trip for every message type that
// GameConnectionController::OnManagerRawMessage handles.  OnManagerRawMessage
// is private, so we cannot call it directly from tests.  However, the codec
// path (Dia::Proto::ToJson / FromJson) that the controller relies on is public,
// and the dispatch switch is purely driven by DebugMessage::payload_case().
//
// By verifying that each message type serialises to JSON and deserialises back
// with the correct payload_case and field values, we cover the same parsing
// logic that runs inside the controller at runtime.
// ---------------------------------------------------------------------------

// ---- GameInfo ----

TEST(GameConnectionControllerMessages, ProtoGameInfo_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	outMsg.set_timestamp(1000);
	auto* info = outMsg.mutable_game_info();
	info->set_name("TestGame");
	info->set_build("1.0.0");
	info->set_processing_unit_count(4);
	info->set_current_phase("Running");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kGameInfo);
	EXPECT_EQ(inMsg.type(), dia::debug::MESSAGE_TYPE_GAME_INFO);
	EXPECT_EQ(inMsg.timestamp(), 1000u);

	const auto& gi = inMsg.game_info();
	EXPECT_EQ(gi.name(), "TestGame");
	EXPECT_EQ(gi.build(), "1.0.0");
	EXPECT_EQ(gi.processing_unit_count(), 4);
	EXPECT_EQ(gi.current_phase(), "Running");
}

// ---- Pong ----

TEST(GameConnectionControllerMessages, ProtoPong_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_PONG);
	outMsg.mutable_pong()->set_ts(9876543210ULL);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kPong);
	EXPECT_EQ(inMsg.pong().ts(), 9876543210ULL);
}

// ---- Log ----

TEST(GameConnectionControllerMessages, ProtoLog_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_LOG);
	auto* log = outMsg.mutable_log();
	log->set_level("warning");
	log->set_channel("Physics");
	log->set_message("Overlap detected on entity 42");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kLog);

	const auto& l = inMsg.log();
	EXPECT_EQ(l.level(), "warning");
	EXPECT_EQ(l.channel(), "Physics");
	EXPECT_EQ(l.message(), "Overlap detected on entity 42");
}

// ---- LogBatch ----

TEST(GameConnectionControllerMessages, ProtoLogBatch_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_LOG_BATCH);
	auto* batch = outMsg.mutable_log_batch();

	auto* e1 = batch->add_entries();
	e1->set_level("info");
	e1->set_channel("Core");
	e1->set_message("First entry");

	auto* e2 = batch->add_entries();
	e2->set_level("error");
	e2->set_channel("Render");
	e2->set_message("Second entry");

	auto* e3 = batch->add_entries();
	e3->set_level("debug");
	e3->set_channel("Audio");
	e3->set_message("Third entry");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kLogBatch);
	ASSERT_EQ(inMsg.log_batch().entries_size(), 3);

	EXPECT_EQ(inMsg.log_batch().entries(0).level(), "info");
	EXPECT_EQ(inMsg.log_batch().entries(0).channel(), "Core");
	EXPECT_EQ(inMsg.log_batch().entries(0).message(), "First entry");

	EXPECT_EQ(inMsg.log_batch().entries(1).level(), "error");
	EXPECT_EQ(inMsg.log_batch().entries(1).channel(), "Render");
	EXPECT_EQ(inMsg.log_batch().entries(1).message(), "Second entry");

	EXPECT_EQ(inMsg.log_batch().entries(2).level(), "debug");
	EXPECT_EQ(inMsg.log_batch().entries(2).channel(), "Audio");
	EXPECT_EQ(inMsg.log_batch().entries(2).message(), "Third entry");
}

// ---- CoreMetrics ----

TEST(GameConnectionControllerMessages, ProtoCoreMetrics_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
	auto* cm = outMsg.mutable_core_metrics();
	cm->set_fps(60.0f);
	cm->set_frame_time_ms(16.6f);
	cm->set_memory_used_mb(512.0f);
	cm->set_memory_available_mb(2048.0f);
	cm->set_uptime_seconds(3600.0f);

	auto* pu1 = cm->add_processing_units();
	pu1->set_name("MainPU");
	pu1->set_fps(60.0f);
	pu1->set_frame_time_ms(16.5f);

	auto* pu2 = cm->add_processing_units();
	pu2->set_name("RenderPU");
	pu2->set_fps(120.0f);
	pu2->set_frame_time_ms(8.3f);

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kCoreMetrics);

	const auto& metrics = inMsg.core_metrics();
	EXPECT_FLOAT_EQ(metrics.fps(), 60.0f);
	EXPECT_FLOAT_EQ(metrics.frame_time_ms(), 16.6f);
	EXPECT_FLOAT_EQ(metrics.memory_used_mb(), 512.0f);
	EXPECT_FLOAT_EQ(metrics.memory_available_mb(), 2048.0f);
	EXPECT_FLOAT_EQ(metrics.uptime_seconds(), 3600.0f);

	ASSERT_EQ(metrics.processing_units_size(), 2);
	EXPECT_EQ(metrics.processing_units(0).name(), "MainPU");
	EXPECT_FLOAT_EQ(metrics.processing_units(0).fps(), 60.0f);
	EXPECT_FLOAT_EQ(metrics.processing_units(0).frame_time_ms(), 16.5f);

	EXPECT_EQ(metrics.processing_units(1).name(), "RenderPU");
	EXPECT_FLOAT_EQ(metrics.processing_units(1).fps(), 120.0f);
	EXPECT_FLOAT_EQ(metrics.processing_units(1).frame_time_ms(), 8.3f);
}

// ---- HandshakeResponse ----

TEST(GameConnectionControllerMessages, ProtoHandshakeResponse_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
	auto* hs = outMsg.mutable_handshake_response();
	hs->set_protocol_version(1);
	hs->set_accepted(true);
	hs->set_server_name("DiaDebugServer");
	hs->set_server_version("2.3.1");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kHandshakeResponse);

	const auto& hr = inMsg.handshake_response();
	EXPECT_EQ(hr.protocol_version(), 1);
	EXPECT_TRUE(hr.accepted());
	EXPECT_EQ(hr.server_name(), "DiaDebugServer");
	EXPECT_EQ(hr.server_version(), "2.3.1");
}

// ---- CommandResponse ----

TEST(GameConnectionControllerMessages, ProtoCommandResponse_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	auto* cr = outMsg.mutable_command_response();
	cr->set_command("reload_shaders");
	cr->set_success(true);
	cr->set_message("14 shaders recompiled");
	cr->set_payload_json("{\"count\":14}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kCommandResponse);

	const auto& resp = inMsg.command_response();
	EXPECT_EQ(resp.command(), "reload_shaders");
	EXPECT_TRUE(resp.success());
	EXPECT_EQ(resp.message(), "14 shaders recompiled");
	EXPECT_EQ(resp.payload_json(), "{\"count\":14}");
}

// ---- Event ----

TEST(GameConnectionControllerMessages, ProtoEvent_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_EVENT);
	auto* ev = outMsg.mutable_event();
	ev->set_event_type("entity_spawned");
	ev->set_payload_json("{\"entity_id\":99,\"type\":\"NPC\"}");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kEvent);

	const auto& e = inMsg.event();
	EXPECT_EQ(e.event_type(), "entity_spawned");
	EXPECT_EQ(e.payload_json(), "{\"entity_id\":99,\"type\":\"NPC\"}");
}

// ---- Error ----

TEST(GameConnectionControllerMessages, ProtoError_ParsesCorrectly)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_ERROR);
	auto* err = outMsg.mutable_error();
	err->set_error_code("INVALID_COMMAND");
	err->set_message("Command 'foo' is not registered");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));
	ASSERT_GT(strlen(json), 0u);

	dia::debug::DebugMessage inMsg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &inMsg));

	EXPECT_EQ(inMsg.payload_case(), dia::debug::DebugMessage::kError);

	const auto& error = inMsg.error();
	EXPECT_EQ(error.error_code(), "INVALID_COMMAND");
	EXPECT_EQ(error.message(), "Command 'foo' is not registered");
}

// ---------------------------------------------------------------------------
// Error / edge-case handling
// ---------------------------------------------------------------------------

TEST(GameConnectionControllerMessages, EmptyJson_FailsGracefully)
{
	dia::debug::DebugMessage msg;
	EXPECT_FALSE(Dia::Proto::FromJson("", &msg));
}

TEST(GameConnectionControllerMessages, GarbageText_FailsGracefully)
{
	dia::debug::DebugMessage msg;
	EXPECT_FALSE(Dia::Proto::FromJson("not json", &msg));
}

TEST(GameConnectionControllerMessages, ValidJsonButNotProto_ParsesWithDefaultPayload)
{
	// ignore_unknown_fields is true in the codec, so an unknown field should
	// parse without error but leave the oneof payload unset.
	dia::debug::DebugMessage msg;
	ASSERT_TRUE(Dia::Proto::FromJson("{\"unknown_field\": 42}", &msg));
	EXPECT_EQ(msg.payload_case(), dia::debug::DebugMessage::PAYLOAD_NOT_SET);
}

// ---------------------------------------------------------------------------
// Wire-format compatibility: json_name annotations and field-name forms
// ---------------------------------------------------------------------------

// ToJson with preserve_proto_field_names=true should emit the proto field
// name (snake_case), NOT the json_name annotation (camelCase).
TEST(GameConnectionControllerMessages, GameInfo_ToJson_UsesSnakeCaseFieldNames)
{
	dia::debug::DebugMessage outMsg;
	outMsg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	auto* info = outMsg.mutable_game_info();
	info->set_name("WireTest");
	info->set_build("0.0.1");
	info->set_processing_unit_count(2);
	info->set_current_phase("Init");

	char json[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(outMsg, json, sizeof(json)));

	std::string jsonStr(json);
	// With preserve_proto_field_names the codec should emit snake_case.
	EXPECT_NE(jsonStr.find("processing_unit_count"), std::string::npos)
		<< "Expected snake_case field name in JSON output. Got: " << jsonStr;
	EXPECT_NE(jsonStr.find("current_phase"), std::string::npos)
		<< "Expected snake_case field name in JSON output. Got: " << jsonStr;
}

// FromJson must accept camelCase (the json_name annotation form).  The JS UI
// may send messages using this convention.
TEST(GameConnectionControllerMessages, GameInfo_FromJson_AcceptsCamelCase)
{
	const char* json = R"({
		"type": "MESSAGE_TYPE_GAME_INFO",
		"game_info": {
			"name": "CamelTest",
			"build": "1.2.3",
			"processingUnitCount": 7,
			"currentPhase": "Gameplay"
		}
	})";

	dia::debug::DebugMessage msg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &msg));
	EXPECT_EQ(msg.payload_case(), dia::debug::DebugMessage::kGameInfo);

	const auto& gi = msg.game_info();
	EXPECT_EQ(gi.name(), "CamelTest");
	EXPECT_EQ(gi.build(), "1.2.3");
	EXPECT_EQ(gi.processing_unit_count(), 7);
	EXPECT_EQ(gi.current_phase(), "Gameplay");
}

// FromJson must also accept the raw proto field names (snake_case).  The game
// server serialises with preserve_proto_field_names=true.
TEST(GameConnectionControllerMessages, GameInfo_FromJson_AcceptsSnakeCase)
{
	const char* json = R"({
		"type": "MESSAGE_TYPE_GAME_INFO",
		"game_info": {
			"name": "SnakeTest",
			"build": "4.5.6",
			"processing_unit_count": 3,
			"current_phase": "Loading"
		}
	})";

	dia::debug::DebugMessage msg;
	ASSERT_TRUE(Dia::Proto::FromJson(json, &msg));
	EXPECT_EQ(msg.payload_case(), dia::debug::DebugMessage::kGameInfo);

	const auto& gi = msg.game_info();
	EXPECT_EQ(gi.name(), "SnakeTest");
	EXPECT_EQ(gi.build(), "4.5.6");
	EXPECT_EQ(gi.processing_unit_count(), 3);
	EXPECT_EQ(gi.current_phase(), "Loading");
}
