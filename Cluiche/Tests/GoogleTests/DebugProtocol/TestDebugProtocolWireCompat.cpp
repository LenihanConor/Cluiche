#include <gtest/gtest.h>

#include "DiaDebugProtocol/DiaDebugProtocol.h"
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <sstream>

// ============================================================================
// Helper: jsoncpp round-trip using FastWriter (the production path)
//
// This is the exact sequence that GameConnectionManager::HandleMessage uses:
//   server builds proto -> ToJson() -> network (string) -> jsoncpp parse
//   -> jsoncpp re-serialize (FastWriter) -> FromJson()
//
// FastWriter appends a trailing '\n' which may break protobuf JSON parsing.
// ============================================================================

static std::string JsoncppRoundTrip_FastWriter(const char* json)
{
	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(json);
	if (!Json::parseFromStream(builder, stream, &parsed, &errors))
		return "";
	return Json::FastWriter().write(parsed);
}

static std::string JsoncppRoundTrip_StreamWriter(const char* json)
{
	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(json);
	if (!Json::parseFromStream(builder, stream, &parsed, &errors))
		return "";
	Json::StreamWriterBuilder writerBuilder;
	writerBuilder["indentation"] = "";
	return Json::writeString(writerBuilder, parsed);
}

// ============================================================================
// 1. Full wire round-trip: ToJson -> jsoncpp parse -> FastWriter -> FromJson
//    These tests exercise THE exact bug path.
// ============================================================================

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_GameInfo)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	original.set_timestamp(9999ULL);
	auto* info = original.mutable_game_info();
	info->set_name("TestGame");
	info->set_build("2.1.0");
	info->set_processing_unit_count(4);
	info->set_current_phase("Running");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	// Simulate jsoncpp round-trip (what GameConnectionManager::HandleMessage does)
	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	// Parse back to proto
	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_GAME_INFO);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kGameInfo);
	EXPECT_EQ(result.game_info().name(), "TestGame");
	EXPECT_EQ(result.game_info().build(), "2.1.0");
	EXPECT_EQ(result.game_info().processing_unit_count(), 4);
	EXPECT_EQ(result.game_info().current_phase(), "Running");
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_HandshakeResponse)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
	original.set_timestamp(1000ULL);
	auto* resp = original.mutable_handshake_response();
	resp->set_protocol_version(1);
	resp->set_accepted(true);
	resp->set_server_name("DiaDebugServer");
	resp->set_server_version("1.0.0");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kHandshakeResponse);
	EXPECT_EQ(result.handshake_response().protocol_version(), 1);
	EXPECT_TRUE(result.handshake_response().accepted());
	EXPECT_EQ(result.handshake_response().server_name(), "DiaDebugServer");
	EXPECT_EQ(result.handshake_response().server_version(), "1.0.0");
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_Pong)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_PONG);
	original.set_timestamp(5000ULL);
	original.mutable_pong()->set_ts(1234567890ULL);

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_PONG);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kPong);
	EXPECT_EQ(result.pong().ts(), 1234567890ULL);
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_CoreMetrics)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
	original.set_timestamp(7777ULL);
	auto* cm = original.mutable_core_metrics();
	cm->set_fps(60.0f);
	cm->set_frame_time_ms(16.67f);
	cm->set_memory_used_mb(512.0f);
	cm->set_memory_available_mb(2048.0f);
	cm->set_uptime_seconds(300.5f);

	auto* pu1 = cm->add_processing_units();
	pu1->set_name("MainPU");
	pu1->set_fps(60.0f);
	pu1->set_frame_time_ms(16.67f);

	auto* pu2 = cm->add_processing_units();
	pu2->set_name("RenderPU");
	pu2->set_fps(59.5f);
	pu2->set_frame_time_ms(16.81f);

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_CORE_METRICS);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kCoreMetrics);
	EXPECT_FLOAT_EQ(result.core_metrics().fps(), 60.0f);
	EXPECT_FLOAT_EQ(result.core_metrics().frame_time_ms(), 16.67f);
	EXPECT_FLOAT_EQ(result.core_metrics().memory_used_mb(), 512.0f);
	EXPECT_FLOAT_EQ(result.core_metrics().memory_available_mb(), 2048.0f);
	EXPECT_FLOAT_EQ(result.core_metrics().uptime_seconds(), 300.5f);
	EXPECT_EQ(result.core_metrics().processing_units_size(), 2);
	EXPECT_EQ(result.core_metrics().processing_units(0).name(), "MainPU");
	EXPECT_EQ(result.core_metrics().processing_units(1).name(), "RenderPU");
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_Log)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_LOG);
	original.set_timestamp(2000ULL);
	auto* log = original.mutable_log();
	log->set_level("warning");
	log->set_channel("Render");
	log->set_message("Frame drop detected");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_LOG);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kLog);
	EXPECT_EQ(result.log().level(), "warning");
	EXPECT_EQ(result.log().channel(), "Render");
	EXPECT_EQ(result.log().message(), "Frame drop detected");
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_LogBatch)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_LOG_BATCH);
	original.set_timestamp(3000ULL);
	auto* batch = original.mutable_log_batch();

	auto* e1 = batch->add_entries();
	e1->set_level("info");
	e1->set_channel("Core");
	e1->set_message("Engine started");

	auto* e2 = batch->add_entries();
	e2->set_level("error");
	e2->set_channel("Net");
	e2->set_message("Connection timeout");

	auto* e3 = batch->add_entries();
	e3->set_level("debug");
	e3->set_channel("Physics");
	e3->set_message("Step complete");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_LOG_BATCH);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kLogBatch);
	EXPECT_EQ(result.log_batch().entries_size(), 3);
	EXPECT_EQ(result.log_batch().entries(0).level(), "info");
	EXPECT_EQ(result.log_batch().entries(0).channel(), "Core");
	EXPECT_EQ(result.log_batch().entries(0).message(), "Engine started");
	EXPECT_EQ(result.log_batch().entries(1).level(), "error");
	EXPECT_EQ(result.log_batch().entries(1).message(), "Connection timeout");
	EXPECT_EQ(result.log_batch().entries(2).level(), "debug");
	EXPECT_EQ(result.log_batch().entries(2).channel(), "Physics");
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_CommandResponse)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	original.set_timestamp(4000ULL);
	auto* resp = original.mutable_command_response();
	resp->set_command("hot_reload");
	resp->set_success(true);
	resp->set_message("Reload completed successfully");
	resp->set_payload_json("{\"modules_reloaded\":3}");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kCommandResponse);
	EXPECT_EQ(result.command_response().command(), "hot_reload");
	EXPECT_TRUE(result.command_response().success());
	EXPECT_EQ(result.command_response().message(), "Reload completed successfully");
	EXPECT_EQ(result.command_response().payload_json(), "{\"modules_reloaded\":3}");
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_Event)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_EVENT);
	original.set_timestamp(5500ULL);
	auto* evt = original.mutable_event();
	evt->set_event_type("phase_transition");
	evt->set_payload_json("{\"from\":\"Init\",\"to\":\"Update\"}");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_EVENT);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kEvent);
	EXPECT_EQ(result.event().event_type(), "phase_transition");
	EXPECT_EQ(result.event().payload_json(), "{\"from\":\"Init\",\"to\":\"Update\"}");
}

TEST(DebugProtocolWireCompat, ToJson_ThenJsoncppParse_ThenFromJson_DataUpdate)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
	original.set_timestamp(6000ULL);
	auto* update = original.mutable_data_update();
	update->set_data_type("processing_unit_state");
	update->set_payload_json("{\"pu_id\":\"MainPU\",\"phase\":\"Update\"}");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	Json::Value parsed;
	Json::CharReaderBuilder builder;
	std::string errors;
	std::istringstream stream(protoJson);
	ASSERT_TRUE(Json::parseFromStream(builder, stream, &parsed, &errors));
	std::string reserializedJson = Json::FastWriter().write(parsed);

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_DATA_UPDATE);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kDataUpdate);
	EXPECT_EQ(result.data_update().data_type(), "processing_unit_state");
	EXPECT_EQ(result.data_update().payload_json(), "{\"pu_id\":\"MainPU\",\"phase\":\"Update\"}");
}

// ============================================================================
// 2. Trailing newline tests: FastWriter appends '\n' which may break proto
// ============================================================================

TEST(DebugProtocolWireCompat, FastWriter_TrailingNewline_ProtoCanParse)
{
	// FastWriter always appends a trailing '\n' to its output.
	// This test verifies whether protobuf's JSON parser tolerates it.
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	auto* info = original.mutable_game_info();
	info->set_name("TrailingNewlineTest");
	info->set_build("1.0");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	// Manually append trailing newline to simulate what FastWriter does
	std::string withNewline = std::string(protoJson) + "\n";

	dia::debug::DebugMessage result;
	bool parseOk = Dia::Proto::FromJson(withNewline.c_str(), &result);

	// Document the actual behavior: whether protobuf tolerates trailing whitespace.
	// If this EXPECT_TRUE fails, it means FromJson rejects trailing newlines and
	// the jsoncpp FastWriter path is broken in production.
	EXPECT_TRUE(parseOk)
		<< "CRITICAL: protobuf FromJson rejects trailing newline. "
		   "FastWriter output will fail in production. "
		   "Input was: [" << withNewline << "]";

	if (parseOk)
	{
		EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_GAME_INFO);
		EXPECT_EQ(result.game_info().name(), "TrailingNewlineTest");
	}
}

// ============================================================================
// 3. StreamWriter (no trailing newline) as a safer alternative
// ============================================================================

TEST(DebugProtocolWireCompat, StreamWriter_NoTrailingNewline_ProtoCanParse)
{
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	auto* info = original.mutable_game_info();
	info->set_name("StreamWriterTest");
	info->set_build("2.0");
	info->set_processing_unit_count(1);
	info->set_current_phase("Init");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));
	std::string streamWriterJson = JsoncppRoundTrip_StreamWriter(protoJson);
	ASSERT_FALSE(streamWriterJson.empty());

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(streamWriterJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_GAME_INFO);
	EXPECT_EQ(result.payload_case(), dia::debug::DebugMessage::kGameInfo);
	EXPECT_EQ(result.game_info().name(), "StreamWriterTest");
	EXPECT_EQ(result.game_info().build(), "2.0");
	EXPECT_EQ(result.game_info().processing_unit_count(), 1);
	EXPECT_EQ(result.game_info().current_phase(), "Init");
}

// ============================================================================
// 4. Enum string preservation through jsoncpp round-trip
// ============================================================================

TEST(DebugProtocolWireCompat, JsoncppRoundTrip_PreservesEnumAsString)
{
	// Protobuf JSON serialization emits enums as strings like "MESSAGE_TYPE_GAME_INFO".
	// Verify that jsoncpp preserves the string representation and FromJson accepts it.
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	original.mutable_game_info()->set_name("EnumTest");

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	std::string protoJsonStr(protoJson);

	// Verify the raw proto JSON contains the enum as a string
	EXPECT_NE(protoJsonStr.find("\"MESSAGE_TYPE_GAME_INFO\""), std::string::npos)
		<< "Proto ToJson should emit enum as string. Got: " << protoJsonStr;

	// Round-trip through jsoncpp
	std::string reserializedJson = JsoncppRoundTrip_FastWriter(protoJson);
	ASSERT_FALSE(reserializedJson.empty());

	// Verify jsoncpp preserved the enum string
	EXPECT_NE(reserializedJson.find("MESSAGE_TYPE_GAME_INFO"), std::string::npos)
		<< "jsoncpp should preserve enum string. Got: " << reserializedJson;

	// Verify FromJson still accepts it
	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_EQ(result.type(), dia::debug::MESSAGE_TYPE_GAME_INFO);
}

// ============================================================================
// 5. uint64 string representation preservation
// ============================================================================

TEST(DebugProtocolWireCompat, JsoncppRoundTrip_PreservesUint64)
{
	// Protobuf JSON spec encodes uint64 as a string (e.g., "12345678901234").
	// jsoncpp must preserve this string representation, not convert it to a number.
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_PING);
	original.set_timestamp(9876543210123ULL);
	original.mutable_ping()->set_ts(18446744073709551000ULL); // near max uint64

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	// Round-trip through jsoncpp
	std::string reserializedJson = JsoncppRoundTrip_FastWriter(protoJson);
	ASSERT_FALSE(reserializedJson.empty());

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result))
		<< "FromJson failed after jsoncpp round-trip. Reserialized: " << reserializedJson;

	EXPECT_EQ(result.timestamp(), 9876543210123ULL);
	EXPECT_EQ(result.ping().ts(), 18446744073709551000ULL);
}

// ============================================================================
// 6. Float precision preservation
// ============================================================================

TEST(DebugProtocolWireCompat, JsoncppRoundTrip_PreservesFloatPrecision)
{
	// CoreMetrics with fps=59.94f. Verify after jsoncpp round-trip,
	// FromJson recovers a value within FLOAT_EQ tolerance.
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
	auto* cm = original.mutable_core_metrics();
	cm->set_fps(59.94f);
	cm->set_frame_time_ms(16.6833f);
	cm->set_memory_used_mb(1023.75f);

	char protoJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, protoJson, sizeof(protoJson)));

	std::string reserializedJson = JsoncppRoundTrip_FastWriter(protoJson);
	ASSERT_FALSE(reserializedJson.empty());

	dia::debug::DebugMessage result;
	EXPECT_TRUE(Dia::Proto::FromJson(reserializedJson.c_str(), &result));
	EXPECT_FLOAT_EQ(result.core_metrics().fps(), 59.94f);
	EXPECT_FLOAT_EQ(result.core_metrics().frame_time_ms(), 16.6833f);
	EXPECT_FLOAT_EQ(result.core_metrics().memory_used_mb(), 1023.75f);
}

// ============================================================================
// 7. Direct raw text vs jsoncpp round-trip: both must parse identically
// ============================================================================

TEST(DebugProtocolWireCompat, DirectRawText_VsJsoncppRoundTrip_BothParse)
{
	// Build a GameInfo proto, ToJson to get raw text. Parse with jsoncpp and
	// re-serialize. Call FromJson on both. Verify both succeed and produce
	// identical payload_case and field values.
	// This proves whether the jsoncpp round-trip is lossy.
	dia::debug::DebugMessage original;
	original.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
	original.set_timestamp(42ULL);
	auto* info = original.mutable_game_info();
	info->set_name("ComparisonTest");
	info->set_build("3.0.0");
	info->set_processing_unit_count(7);
	info->set_current_phase("Shutdown");

	char rawJson[4096];
	ASSERT_TRUE(Dia::Proto::ToJson(original, rawJson, sizeof(rawJson)));
	std::string roundTrippedJson = JsoncppRoundTrip_FastWriter(rawJson);
	ASSERT_FALSE(roundTrippedJson.empty());

	// Parse from raw proto JSON (direct path)
	dia::debug::DebugMessage fromRaw;
	ASSERT_TRUE(Dia::Proto::FromJson(rawJson, &fromRaw))
		<< "FromJson failed on raw proto JSON: " << rawJson;

	// Parse from jsoncpp round-tripped JSON (production path)
	dia::debug::DebugMessage fromRoundTrip;
	ASSERT_TRUE(Dia::Proto::FromJson(roundTrippedJson.c_str(), &fromRoundTrip))
		<< "FromJson failed on jsoncpp round-tripped JSON: " << roundTrippedJson;

	// Both must produce identical results
	EXPECT_EQ(fromRaw.type(), fromRoundTrip.type());
	EXPECT_EQ(fromRaw.payload_case(), fromRoundTrip.payload_case());
	EXPECT_EQ(fromRaw.payload_case(), dia::debug::DebugMessage::kGameInfo);

	EXPECT_EQ(fromRaw.game_info().name(), fromRoundTrip.game_info().name());
	EXPECT_EQ(fromRaw.game_info().build(), fromRoundTrip.game_info().build());
	EXPECT_EQ(fromRaw.game_info().processing_unit_count(),
	          fromRoundTrip.game_info().processing_unit_count());
	EXPECT_EQ(fromRaw.game_info().current_phase(),
	          fromRoundTrip.game_info().current_phase());

	// Verify actual values (not just equality between the two)
	EXPECT_EQ(fromRoundTrip.game_info().name(), "ComparisonTest");
	EXPECT_EQ(fromRoundTrip.game_info().build(), "3.0.0");
	EXPECT_EQ(fromRoundTrip.game_info().processing_unit_count(), 7);
	EXPECT_EQ(fromRoundTrip.game_info().current_phase(), "Shutdown");
}
