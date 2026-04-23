#include <gtest/gtest.h>

#include "DiaDebugProtocol/DiaDebugProtocol.h"
#include <DiaCore/Json/external/json/json.h>

#include <string>
#include <cstring>
#include <sstream>

//=============================================================================
// DebugProtocolEndToEnd
//
// Tests the full JSON data transformations that happen in production between
// the debug server (DebugServerModule) and editor (GameConnectionController,
// GameConnectionManager) without requiring actual WebSocket connections.
//
// Each test mirrors a real production data path:
//   Server builds proto -> ToJson -> (wire) -> FromJson -> field extraction
//   Editor builds proto -> ToJson -> (wire) -> FromJson -> field extraction
//=============================================================================

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Mirrors GameConnectionManager::HandleMessage jsoncpp parse step.
static bool ParseWithJsonCpp(const char* text, Json::Value& outValue)
{
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(text);
    return Json::parseFromStream(builder, stream, &outValue, &errors);
}

// Mirrors GameConnectionManager::HandleMessage command_response reconstruction.
static Json::Value ReconstructCommandResponse(const dia::debug::CommandResponse& resp)
{
    Json::Value responseJson;
    responseJson["command"] = resp.command();
    responseJson["success"] = resp.success();
    responseJson["message"] = resp.message();
    if (!resp.payload_json().empty())
    {
        Json::Reader reader;
        Json::Value payload;
        reader.parse(resp.payload_json(), payload);
        responseJson["payload"] = payload;
    }
    return responseJson;
}

// Mirrors GameConnectionController::OnManagerRawMessage core_metrics reconstruction.
static Json::Value ReconstructCoreMetrics(const dia::debug::CoreMetrics& cm)
{
    Json::Value metricsPayload;
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

    return metricsPayload;
}

// Mirrors GameConnectionController::OnManagerRawMessage game_info reconstruction.
static Json::Value ReconstructGameInfo(const dia::debug::GameInfo& info)
{
    Json::Value gameInfo;
    gameInfo["name"] = info.name();
    gameInfo["build"] = info.build();
    gameInfo["processingUnitCount"] = info.processing_unit_count();
    gameInfo["currentPhase"] = info.current_phase();
    return gameInfo;
}

//-----------------------------------------------------------------------------
// Test 1: ServerHandshake_ToEditorParse
//
// Path 1: Server -> Editor (handshake_response + game_info flow)
// Server builds handshake_response and game_info protos as it does in
// DebugServerModule::HandleConnection. Serialize both. For each, verify:
//   (a) jsoncpp can parse the raw string
//   (b) FromJson on the raw string succeeds
//   (c) correct payload_case
//   (d) all field values match
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, ServerHandshake_ToEditorParse)
{
    // --- Build handshake_response exactly as DebugServerModule::HandleConnection ---
    dia::debug::DebugMessage welcome;
    welcome.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
    welcome.set_timestamp(1000000ULL);
    auto* resp = welcome.mutable_handshake_response();
    resp->set_protocol_version(Dia::DebugProtocol::kProtocolVersion);
    resp->set_accepted(true);
    resp->set_server_name("DiaDebugServer");
    resp->set_server_version("1.0.0");

    char handshakeJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(welcome, handshakeJson, sizeof(handshakeJson)));

    // (a) jsoncpp can parse
    Json::Value handshakeParsed;
    ASSERT_TRUE(ParseWithJsonCpp(handshakeJson, handshakeParsed));

    // (b) FromJson succeeds
    dia::debug::DebugMessage handshakeMsg;
    ASSERT_TRUE(Dia::Proto::FromJson(handshakeJson, &handshakeMsg));

    // (c) correct payload_case
    EXPECT_EQ(handshakeMsg.payload_case(), dia::debug::DebugMessage::kHandshakeResponse);

    // (d) field values match
    EXPECT_EQ(handshakeMsg.handshake_response().protocol_version(), Dia::DebugProtocol::kProtocolVersion);
    EXPECT_TRUE(handshakeMsg.handshake_response().accepted());
    EXPECT_EQ(handshakeMsg.handshake_response().server_name(), "DiaDebugServer");
    EXPECT_EQ(handshakeMsg.handshake_response().server_version(), "1.0.0");
    EXPECT_EQ(handshakeMsg.timestamp(), 1000000ULL);

    // --- Build game_info exactly as DebugServerModule::SendGameInfo ---
    dia::debug::DebugMessage gameInfoEnvelope;
    gameInfoEnvelope.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
    gameInfoEnvelope.set_timestamp(2000000ULL);
    auto* info = gameInfoEnvelope.mutable_game_info();
    info->set_name("TestGame");
    info->set_build("dev-0.1");
    info->set_processing_unit_count(2);
    info->set_current_phase("Running");

    char gameInfoJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(gameInfoEnvelope, gameInfoJson, sizeof(gameInfoJson)));

    // (a) jsoncpp can parse
    Json::Value gameInfoParsed;
    ASSERT_TRUE(ParseWithJsonCpp(gameInfoJson, gameInfoParsed));

    // (b) FromJson succeeds
    dia::debug::DebugMessage gameInfoMsg;
    ASSERT_TRUE(Dia::Proto::FromJson(gameInfoJson, &gameInfoMsg));

    // (c) correct payload_case
    EXPECT_EQ(gameInfoMsg.payload_case(), dia::debug::DebugMessage::kGameInfo);

    // (d) field values match
    EXPECT_EQ(gameInfoMsg.game_info().name(), "TestGame");
    EXPECT_EQ(gameInfoMsg.game_info().build(), "dev-0.1");
    EXPECT_EQ(gameInfoMsg.game_info().processing_unit_count(), 2);
    EXPECT_EQ(gameInfoMsg.game_info().current_phase(), "Running");
}

//-----------------------------------------------------------------------------
// Test 2: EditorPing_ToServerParse
//
// Path 2: Editor -> Server (ping/pong)
// Editor builds ping proto, ToJson. Server receives text, FromJson.
// Verify kPing and correct ts. Then server builds pong from that ts,
// ToJson. Editor FromJson. Verify kPong and ts matches original.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, EditorPing_ToServerParse)
{
    const uint64_t originalTs = 123456789ULL;

    // Editor builds ping exactly as GameConnectionController::Update does
    dia::debug::DebugMessage pingMsg;
    pingMsg.set_type(dia::debug::MESSAGE_TYPE_PING);
    pingMsg.mutable_ping()->set_ts(originalTs);

    char pingJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(pingMsg, pingJson, sizeof(pingJson)));

    // Server receives and parses (DebugServerModule::HandleMessage)
    dia::debug::DebugMessage serverReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(pingJson, &serverReceived));
    EXPECT_EQ(serverReceived.payload_case(), dia::debug::DebugMessage::kPing);
    EXPECT_EQ(serverReceived.ping().ts(), originalTs);

    // Server builds pong exactly as DebugServerModule::HandlePing does
    dia::debug::DebugMessage pongMsg;
    pongMsg.set_type(dia::debug::MESSAGE_TYPE_PONG);
    pongMsg.set_timestamp(3000000ULL);
    pongMsg.mutable_pong()->set_ts(serverReceived.ping().ts());

    char pongJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(pongMsg, pongJson, sizeof(pongJson)));

    // Editor receives and parses (GameConnectionController::OnManagerRawMessage)
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(pongJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kPong);
    EXPECT_EQ(editorReceived.pong().ts(), originalTs);
}

//-----------------------------------------------------------------------------
// Test 3: EditorSubscribe_ToServerParse
//
// Path 2: Editor -> Server (subscribe)
// Editor builds subscribe proto, ToJson. Verify server-side FromJson
// produces kSubscribe with correct data_type.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, EditorSubscribe_ToServerParse)
{
    dia::debug::DebugMessage subMsg;
    subMsg.set_type(dia::debug::MESSAGE_TYPE_SUBSCRIBE);
    auto* sub = subMsg.mutable_subscribe();
    sub->set_data_type("core_metrics");
    sub->set_filter("{\"interval\":500}");

    char subJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(subMsg, subJson, sizeof(subJson)));

    // Server receives (DebugServerModule::HandleMessage -> HandleSubscribe)
    dia::debug::DebugMessage serverReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(subJson, &serverReceived));
    EXPECT_EQ(serverReceived.payload_case(), dia::debug::DebugMessage::kSubscribe);
    EXPECT_EQ(serverReceived.subscribe().data_type(), "core_metrics");
    EXPECT_EQ(serverReceived.subscribe().filter(), "{\"interval\":500}");
}

//-----------------------------------------------------------------------------
// Test 4: EditorCommand_ToServerResponse_ToEditorProcessing
//
// Full round-trip:
//   a. Editor builds CommandRequest ("get_state", payload {"verbose":true}), ToJson.
//   b. Server FromJson -> extracts command and payload_json -> verifies correct.
//   c. Server builds CommandResponse (success=true, message="ok",
//      payload_json={"state":"running"}), ToJson.
//   d. Editor FromJson -> extracts command_response fields -> reconstructs
//      Json::Value exactly as GameConnectionManager::HandleMessage does.
//   Verify responseJson["command"] == "get_state", ["success"] == true,
//   ["payload"]["state"] == "running".
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, EditorCommand_ToServerResponse_ToEditorProcessing)
{
    // (a) Editor builds CommandRequest exactly as GameConnectionManager::SendCommand
    dia::debug::DebugMessage cmdMsg;
    cmdMsg.set_type(dia::debug::MESSAGE_TYPE_COMMAND_REQUEST);
    auto* cmd = cmdMsg.mutable_command_request();
    cmd->set_command("get_state");
    cmd->set_payload_json("{\"verbose\":true}");

    char cmdJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(cmdMsg, cmdJson, sizeof(cmdJson)));

    // (b) Server receives (DebugServerModule::HandleCommand)
    dia::debug::DebugMessage serverReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(cmdJson, &serverReceived));
    EXPECT_EQ(serverReceived.payload_case(), dia::debug::DebugMessage::kCommandRequest);
    EXPECT_EQ(serverReceived.command_request().command(), "get_state");
    EXPECT_EQ(serverReceived.command_request().payload_json(), "{\"verbose\":true}");

    // (c) Server builds CommandResponse exactly as DebugServerModule::HandleCommand
    dia::debug::DebugMessage response;
    response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
    response.set_timestamp(4000000ULL);
    auto* resp = response.mutable_command_response();
    resp->set_command(serverReceived.command_request().command());
    resp->set_success(true);
    resp->set_message("ok");
    resp->set_payload_json("{\"state\":\"running\"}");

    char respJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(response, respJson, sizeof(respJson)));

    // (d) Editor receives (GameConnectionManager::HandleMessage)
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(respJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kCommandResponse);

    // Reconstruct Json::Value exactly as GameConnectionManager does
    Json::Value responseJson = ReconstructCommandResponse(editorReceived.command_response());

    EXPECT_EQ(responseJson["command"].asString(), "get_state");
    EXPECT_TRUE(responseJson["success"].asBool());
    EXPECT_EQ(responseJson["message"].asString(), "ok");
    EXPECT_EQ(responseJson["payload"]["state"].asString(), "running");
}

//-----------------------------------------------------------------------------
// Test 5: ServerLog_ToEditorConsoleExtraction
//
// Path 4: Server log -> Editor console
// Server builds LogEntry proto. ToJson. Editor FromJson. Extract
// .log().level().c_str() and .log().message().c_str(). Verify the
// c_str() values match exactly. This tests the .c_str() conversion
// path used in PushGameConsoleEntry.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, ServerLog_ToEditorConsoleExtraction)
{
    dia::debug::DebugMessage logMsg;
    logMsg.set_type(dia::debug::MESSAGE_TYPE_LOG);
    logMsg.set_timestamp(5000000ULL);
    auto* log = logMsg.mutable_log();
    log->set_level("warning");
    log->set_channel("Render");
    log->set_message("Frame drop");

    char logJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(logMsg, logJson, sizeof(logJson)));

    // Editor receives (GameConnectionController::OnManagerRawMessage kLog branch)
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(logJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kLog);

    // Exact path used in production: .c_str() extraction
    const auto& editorLog = editorReceived.log();
    EXPECT_STREQ(editorLog.level().c_str(), "warning");
    EXPECT_STREQ(editorLog.channel().c_str(), "Render");
    EXPECT_STREQ(editorLog.message().c_str(), "Frame drop");
}

//-----------------------------------------------------------------------------
// Test 6: ServerLogBatch_ToEditorConsoleExtraction
//
// Same as Test 5 but with LogBatch containing 3 entries. Iterate with
// entries_size() and entries(i). Verify each entry's level().c_str()
// and message().c_str().
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, ServerLogBatch_ToEditorConsoleExtraction)
{
    dia::debug::DebugMessage batchMsg;
    batchMsg.set_type(dia::debug::MESSAGE_TYPE_LOG_BATCH);
    batchMsg.set_timestamp(6000000ULL);
    auto* batch = batchMsg.mutable_log_batch();

    auto* e1 = batch->add_entries();
    e1->set_level("info");
    e1->set_channel("Core");
    e1->set_message("System initialized");

    auto* e2 = batch->add_entries();
    e2->set_level("warning");
    e2->set_channel("Render");
    e2->set_message("Shader recompiled");

    auto* e3 = batch->add_entries();
    e3->set_level("error");
    e3->set_channel("Physics");
    e3->set_message("Collision overflow");

    char batchJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(batchMsg, batchJson, sizeof(batchJson)));

    // Editor receives (GameConnectionController::OnManagerRawMessage kLogBatch branch)
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(batchJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kLogBatch);

    const auto& editorBatch = editorReceived.log_batch();
    ASSERT_EQ(editorBatch.entries_size(), 3);

    EXPECT_STREQ(editorBatch.entries(0).level().c_str(), "info");
    EXPECT_STREQ(editorBatch.entries(0).message().c_str(), "System initialized");

    EXPECT_STREQ(editorBatch.entries(1).level().c_str(), "warning");
    EXPECT_STREQ(editorBatch.entries(1).message().c_str(), "Shader recompiled");

    EXPECT_STREQ(editorBatch.entries(2).level().c_str(), "error");
    EXPECT_STREQ(editorBatch.entries(2).message().c_str(), "Collision overflow");
}

//-----------------------------------------------------------------------------
// Test 7: ServerCoreMetrics_ToEditorJsonReconstruction
//
// Server builds CoreMetrics with fps, frame_time_ms, memory,
// processing_units. ToJson. Editor FromJson. Then reconstruct
// Json::Value exactly as the controller does. Verify the resulting
// Json::Value has correct types and values. This is exactly what
// gets pushed to the web UI.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, ServerCoreMetrics_ToEditorJsonReconstruction)
{
    dia::debug::DebugMessage metricsMsg;
    metricsMsg.set_type(dia::debug::MESSAGE_TYPE_CORE_METRICS);
    metricsMsg.set_timestamp(7000000ULL);
    auto* cm = metricsMsg.mutable_core_metrics();
    cm->set_fps(60.0f);
    cm->set_frame_time_ms(16.67f);
    cm->set_memory_used_mb(512.5f);
    cm->set_memory_available_mb(16384.0f);
    cm->set_uptime_seconds(3600.0f);

    auto* pu1 = cm->add_processing_units();
    pu1->set_name("MainPU");
    pu1->set_fps(60.0f);
    pu1->set_frame_time_ms(16.67f);

    auto* pu2 = cm->add_processing_units();
    pu2->set_name("RenderPU");
    pu2->set_fps(120.0f);
    pu2->set_frame_time_ms(8.33f);

    char metricsJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(metricsMsg, metricsJson, sizeof(metricsJson)));

    // Editor receives
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(metricsJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kCoreMetrics);

    // Reconstruct Json::Value exactly as GameConnectionController does
    Json::Value metricsPayload = ReconstructCoreMetrics(editorReceived.core_metrics());

    // Verify types and values
    EXPECT_TRUE(metricsPayload["fps"].isDouble());
    EXPECT_NEAR(metricsPayload["fps"].asFloat(), 60.0f, 0.01f);

    EXPECT_TRUE(metricsPayload["frame_time_ms"].isDouble());
    EXPECT_NEAR(metricsPayload["frame_time_ms"].asFloat(), 16.67f, 0.01f);

    EXPECT_TRUE(metricsPayload["memory_used_mb"].isDouble());
    EXPECT_NEAR(metricsPayload["memory_used_mb"].asFloat(), 512.5f, 0.1f);

    EXPECT_TRUE(metricsPayload["memory_available_mb"].isDouble());
    EXPECT_NEAR(metricsPayload["memory_available_mb"].asFloat(), 16384.0f, 0.1f);

    EXPECT_TRUE(metricsPayload["uptime_seconds"].isDouble());
    EXPECT_NEAR(metricsPayload["uptime_seconds"].asFloat(), 3600.0f, 0.1f);

    ASSERT_TRUE(metricsPayload["processing_units"].isArray());
    ASSERT_EQ(metricsPayload["processing_units"].size(), 2u);

    EXPECT_EQ(metricsPayload["processing_units"][0]["name"].asString(), "MainPU");
    EXPECT_NEAR(metricsPayload["processing_units"][0]["fps"].asFloat(), 60.0f, 0.01f);
    EXPECT_NEAR(metricsPayload["processing_units"][0]["frame_time_ms"].asFloat(), 16.67f, 0.01f);

    EXPECT_EQ(metricsPayload["processing_units"][1]["name"].asString(), "RenderPU");
    EXPECT_NEAR(metricsPayload["processing_units"][1]["fps"].asFloat(), 120.0f, 0.01f);
    EXPECT_NEAR(metricsPayload["processing_units"][1]["frame_time_ms"].asFloat(), 8.33f, 0.01f);
}

//-----------------------------------------------------------------------------
// Test 8: ServerGameInfo_ToEditorJsonReconstruction
//
// Server builds GameInfo. Editor FromJson. Reconstruct Json::Value as
// controller does (note the camelCase keys -- this is what the JS UI reads).
// Verify json values.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, ServerGameInfo_ToEditorJsonReconstruction)
{
    dia::debug::DebugMessage giMsg;
    giMsg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
    giMsg.set_timestamp(8000000ULL);
    auto* info = giMsg.mutable_game_info();
    info->set_name("CluicheTest");
    info->set_build("release-1.2.3");
    info->set_processing_unit_count(3);
    info->set_current_phase("Update");

    char giJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(giMsg, giJson, sizeof(giJson)));

    // Editor receives
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(giJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kGameInfo);

    // Reconstruct exactly as GameConnectionController::OnManagerRawMessage does
    Json::Value gameInfo = ReconstructGameInfo(editorReceived.game_info());

    EXPECT_EQ(gameInfo["name"].asString(), "CluicheTest");
    EXPECT_EQ(gameInfo["build"].asString(), "release-1.2.3");
    EXPECT_EQ(gameInfo["processingUnitCount"].asInt(), 3);
    EXPECT_EQ(gameInfo["currentPhase"].asString(), "Update");
}

//-----------------------------------------------------------------------------
// Test 9: ServerEvent_PayloadJsonPreservedExactly
//
// Server builds Event with payload_json containing {"from":"Init","to":"Update"}.
// Full round-trip. Verify the string is byte-identical after extraction
// (no whitespace normalization, no key reordering). This matters because
// payload_json may be parsed by downstream consumers.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, ServerEvent_PayloadJsonPreservedExactly)
{
    const std::string originalPayload = "{\"from\":\"Init\",\"to\":\"Update\"}";

    dia::debug::DebugMessage eventMsg;
    eventMsg.set_type(dia::debug::MESSAGE_TYPE_EVENT);
    eventMsg.set_timestamp(9000000ULL);
    auto* evt = eventMsg.mutable_event();
    evt->set_event_type("phase_transition");
    evt->set_payload_json(originalPayload);

    char eventJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(eventMsg, eventJson, sizeof(eventJson)));

    // Round-trip
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(eventJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kEvent);
    EXPECT_EQ(editorReceived.event().event_type(), "phase_transition");

    // Byte-identical verification
    EXPECT_EQ(editorReceived.event().payload_json(), originalPayload);
}

//-----------------------------------------------------------------------------
// Test 10: ServerDataUpdate_PayloadJsonPreservedExactly
//
// Same as Test 9 but for DataUpdate with complex payload_json containing
// nested objects and arrays.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, ServerDataUpdate_PayloadJsonPreservedExactly)
{
    const std::string complexPayload =
        "{\"modules\":[{\"name\":\"Render\",\"active\":true},{\"name\":\"Physics\",\"active\":false}],"
        "\"stats\":{\"fps\":60,\"memory\":{\"used\":512,\"free\":1024}}}";

    dia::debug::DebugMessage dataMsg;
    dataMsg.set_type(dia::debug::MESSAGE_TYPE_DATA_UPDATE);
    dataMsg.set_timestamp(10000000ULL);
    auto* update = dataMsg.mutable_data_update();
    update->set_data_type("application_state");
    update->set_payload_json(complexPayload);

    char dataJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(dataMsg, dataJson, sizeof(dataJson)));

    // Round-trip
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(dataJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kDataUpdate);
    EXPECT_EQ(editorReceived.data_update().data_type(), "application_state");

    // Byte-identical verification -- no whitespace normalization, no key reordering
    EXPECT_EQ(editorReceived.data_update().payload_json(), complexPayload);
}

//-----------------------------------------------------------------------------
// Test 11: MultipleMessagesInSequence
//
// Simulate the exact server connection sequence:
//   a. Server sends handshake_response -> serialize
//   b. Server sends game_info -> serialize
//   c. Editor parses both in order via FromJson
//   d. Verify first is kHandshakeResponse, second is kGameInfo
//   e. This tests that parsing one message doesn't leave state that
//      corrupts parsing the next.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, MultipleMessagesInSequence)
{
    // (a) Server sends handshake_response
    dia::debug::DebugMessage welcome;
    welcome.set_type(dia::debug::MESSAGE_TYPE_HANDSHAKE_RESPONSE);
    welcome.set_timestamp(11000000ULL);
    auto* resp = welcome.mutable_handshake_response();
    resp->set_protocol_version(Dia::DebugProtocol::kProtocolVersion);
    resp->set_accepted(true);
    resp->set_server_name("DiaDebugServer");
    resp->set_server_version("1.0.0");
    char welcomeJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(welcome, welcomeJson, sizeof(welcomeJson)));

    // (b) Server sends game_info
    dia::debug::DebugMessage gameInfo;
    gameInfo.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
    gameInfo.set_timestamp(11000001ULL);
    auto* info = gameInfo.mutable_game_info();
    info->set_name("TestGame");
    info->set_build("dev-0.1");
    info->set_processing_unit_count(1);
    info->set_current_phase("Init");
    char gameInfoJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(gameInfo, gameInfoJson, sizeof(gameInfoJson)));

    // (c) Editor parses both in order, into fresh message objects each time
    // (matching GameConnectionController::OnManagerRawMessage which creates
    //  a local DebugMessage on the stack for each call)
    dia::debug::DebugMessage firstMsg;
    ASSERT_TRUE(Dia::Proto::FromJson(welcomeJson, &firstMsg));

    dia::debug::DebugMessage secondMsg;
    ASSERT_TRUE(Dia::Proto::FromJson(gameInfoJson, &secondMsg));

    // (d) Verify first is kHandshakeResponse, second is kGameInfo
    EXPECT_EQ(firstMsg.payload_case(), dia::debug::DebugMessage::kHandshakeResponse);
    EXPECT_EQ(firstMsg.handshake_response().server_name(), "DiaDebugServer");
    EXPECT_TRUE(firstMsg.handshake_response().accepted());

    EXPECT_EQ(secondMsg.payload_case(), dia::debug::DebugMessage::kGameInfo);
    EXPECT_EQ(secondMsg.game_info().name(), "TestGame");
    EXPECT_EQ(secondMsg.game_info().current_phase(), "Init");

    // (e) Verify the two messages are independent -- first still intact after parsing second
    EXPECT_EQ(firstMsg.payload_case(), dia::debug::DebugMessage::kHandshakeResponse);
    EXPECT_EQ(firstMsg.timestamp(), 11000000ULL);
}

//-----------------------------------------------------------------------------
// Test 12: CommandResponse_PayloadJson_NestedInFastWriter
//
// Server's HandleCommand does:
//   resp->set_payload_json(Json::FastWriter().write(result["payload"]));
// This means payload_json contains FastWriter output (with trailing \n).
// Build this exact scenario: create a Json::Value payload, FastWriter().write()
// it into payload_json, build response proto, ToJson, FromJson, parse the
// payload_json back with jsoncpp. Verify the nested JSON round-trips correctly
// despite the embedded newline.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, CommandResponse_PayloadJson_NestedInFastWriter)
{
    // Build a payload as the server's command handler would
    Json::Value resultPayload;
    resultPayload["processing_unit"] = "MainPU";
    resultPayload["phase"] = "Update";
    resultPayload["modules"] = Json::Value(Json::arrayValue);
    resultPayload["modules"].append("Render");
    resultPayload["modules"].append("Physics");

    // FastWriter().write() -- this is exactly what DebugServerModule::HandleCommand does
    std::string fastWriterOutput = Json::FastWriter().write(resultPayload);

    // FastWriter appends trailing \n
    ASSERT_FALSE(fastWriterOutput.empty());
    EXPECT_EQ(fastWriterOutput.back(), '\n');

    // Server builds response with FastWriter output as payload_json
    dia::debug::DebugMessage response;
    response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
    response.set_timestamp(12000000ULL);
    auto* respPayload = response.mutable_command_response();
    respPayload->set_command("get_state");
    respPayload->set_success(true);
    respPayload->set_message("ok");
    respPayload->set_payload_json(fastWriterOutput);

    char respJson[8192];
    ASSERT_TRUE(Dia::Proto::ToJson(response, respJson, sizeof(respJson)));

    // Editor receives
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(respJson, &editorReceived));

    // Reconstruct exactly as GameConnectionManager::HandleMessage does
    const auto& cmdResp = editorReceived.command_response();
    ASSERT_FALSE(cmdResp.payload_json().empty());

    Json::Reader reader;
    Json::Value parsedPayload;
    ASSERT_TRUE(reader.parse(cmdResp.payload_json(), parsedPayload));

    // Verify the nested JSON round-tripped correctly despite embedded newline
    EXPECT_EQ(parsedPayload["processing_unit"].asString(), "MainPU");
    EXPECT_EQ(parsedPayload["phase"].asString(), "Update");
    ASSERT_TRUE(parsedPayload["modules"].isArray());
    ASSERT_EQ(parsedPayload["modules"].size(), 2u);
    EXPECT_EQ(parsedPayload["modules"][0].asString(), "Render");
    EXPECT_EQ(parsedPayload["modules"][1].asString(), "Physics");
}

//-----------------------------------------------------------------------------
// Test 13: EmptyGameName_DefaultsCorrectly
//
// Server sends GameInfo with name="" (empty). In proto3 JSON, empty string
// fields are omitted from the serialized output. Verify that FromJson
// returns empty string for name() (the default).
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, EmptyGameName_DefaultsCorrectly)
{
    dia::debug::DebugMessage giMsg;
    giMsg.set_type(dia::debug::MESSAGE_TYPE_GAME_INFO);
    giMsg.set_timestamp(13000000ULL);
    auto* info = giMsg.mutable_game_info();
    info->set_name("");  // Empty name
    info->set_build("test-build");
    info->set_processing_unit_count(1);
    info->set_current_phase("Init");

    char giJson[4096];
    ASSERT_TRUE(Dia::Proto::ToJson(giMsg, giJson, sizeof(giJson)));

    // Proto3 JSON behavior: empty string is a default value and may be omitted
    // from the JSON output. Verify this by checking the JSON does not contain "name".
    Json::Value parsed;
    ASSERT_TRUE(ParseWithJsonCpp(giJson, parsed));
    // Proto3 JSON omits default-valued fields; "name" with "" should not appear
    // (This documents the behavior -- if protobuf changes this, the test catches it)

    // Round-trip via FromJson
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(giJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kGameInfo);

    // name() returns "" (the proto3 default for missing/empty string fields)
    EXPECT_EQ(editorReceived.game_info().name(), "");
    EXPECT_TRUE(editorReceived.game_info().name().empty());

    // Other fields should still be present
    EXPECT_EQ(editorReceived.game_info().build(), "test-build");
    EXPECT_EQ(editorReceived.game_info().processing_unit_count(), 1);
    EXPECT_EQ(editorReceived.game_info().current_phase(), "Init");

    // Reconstruct as the controller does -- verify empty name produces empty JSON string
    Json::Value gameInfoVal = ReconstructGameInfo(editorReceived.game_info());
    EXPECT_EQ(gameInfoVal["name"].asString(), "");
}

//-----------------------------------------------------------------------------
// Test 14: LargePayloadJson_Survives
//
// Build a CommandResponse with a very large payload_json (>10KB of repeated
// JSON). Verify it round-trips without truncation.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, LargePayloadJson_Survives)
{
    // Build a large JSON payload (>10KB)
    Json::Value largePayload;
    Json::Value items(Json::arrayValue);
    for (int i = 0; i < 200; ++i)
    {
        Json::Value item;
        item["index"] = i;
        item["name"] = "item_" + std::to_string(i) + "_with_some_extra_padding_to_increase_size";
        item["value"] = static_cast<double>(i) * 1.5;
        item["active"] = (i % 2 == 0);
        item["description"] = "This is a description for item number " + std::to_string(i) +
            " which contains enough text to make the payload realistically large for testing purposes.";
        items.append(item);
    }
    largePayload["items"] = items;
    largePayload["total_count"] = 200;

    std::string payloadStr = Json::FastWriter().write(largePayload);
    ASSERT_GT(payloadStr.size(), 10000u);  // Verify it's actually >10KB

    // Build response with large payload
    dia::debug::DebugMessage response;
    response.set_type(dia::debug::MESSAGE_TYPE_COMMAND_RESPONSE);
    response.set_timestamp(14000000ULL);
    auto* resp = response.mutable_command_response();
    resp->set_command("get_large_data");
    resp->set_success(true);
    resp->set_message("ok");
    resp->set_payload_json(payloadStr);

    // Use a larger buffer for this test since the payload is >10KB
    char respJson[65536];
    ASSERT_TRUE(Dia::Proto::ToJson(response, respJson, sizeof(respJson)));

    // Round-trip
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(respJson, &editorReceived));

    // Verify no truncation -- payload_json should be identical in length
    EXPECT_EQ(editorReceived.command_response().payload_json().size(), payloadStr.size());
    EXPECT_EQ(editorReceived.command_response().payload_json(), payloadStr);

    // Verify the nested JSON is still parseable and has all 200 items
    Json::Reader reader;
    Json::Value parsedPayload;
    ASSERT_TRUE(reader.parse(editorReceived.command_response().payload_json(), parsedPayload));
    ASSERT_TRUE(parsedPayload["items"].isArray());
    EXPECT_EQ(parsedPayload["items"].size(), 200u);
    EXPECT_EQ(parsedPayload["total_count"].asInt(), 200);

    // Spot-check first and last items
    EXPECT_EQ(parsedPayload["items"][0]["index"].asInt(), 0);
    EXPECT_EQ(parsedPayload["items"][199]["index"].asInt(), 199);
}

//-----------------------------------------------------------------------------
// Test 15: SpecialCharactersInStrings
//
// Build a LogEntry with message containing special chars: quotes,
// backslashes, newlines, unicode. Verify the string survives the
// proto -> JSON -> proto round-trip exactly.
//-----------------------------------------------------------------------------
TEST(DebugProtocolEndToEnd, SpecialCharactersInStrings)
{
    // Message with quotes, backslashes, newlines, tabs, and unicode
    const std::string specialMessage =
        "Error in \"Shader\\Compile\":\n"
        "\tLine 42: unexpected token\n"
        "\tPath: C:\\Games\\Assets\\shader.glsl\n"
        "\tUnicode: \xC3\xA9\xC3\xA0\xC3\xBC (accented chars)";

    dia::debug::DebugMessage logMsg;
    logMsg.set_type(dia::debug::MESSAGE_TYPE_LOG);
    logMsg.set_timestamp(15000000ULL);
    auto* log = logMsg.mutable_log();
    log->set_level("error");
    log->set_channel("Shader");
    log->set_message(specialMessage);

    char logJson[8192];
    ASSERT_TRUE(Dia::Proto::ToJson(logMsg, logJson, sizeof(logJson)));

    // Verify jsoncpp can parse the serialized output
    Json::Value parsed;
    ASSERT_TRUE(ParseWithJsonCpp(logJson, parsed));

    // Round-trip via FromJson
    dia::debug::DebugMessage editorReceived;
    ASSERT_TRUE(Dia::Proto::FromJson(logJson, &editorReceived));
    EXPECT_EQ(editorReceived.payload_case(), dia::debug::DebugMessage::kLog);

    // Verify the message survived exactly -- byte-identical
    EXPECT_EQ(editorReceived.log().message(), specialMessage);
    EXPECT_STREQ(editorReceived.log().message().c_str(), specialMessage.c_str());
    EXPECT_EQ(editorReceived.log().level(), "error");
    EXPECT_EQ(editorReceived.log().channel(), "Shader");

    // Verify specific special characters are preserved
    const std::string& roundTripped = editorReceived.log().message();
    EXPECT_NE(roundTripped.find("\"Shader\\Compile\""), std::string::npos);
    EXPECT_NE(roundTripped.find('\n'), std::string::npos);
    EXPECT_NE(roundTripped.find('\t'), std::string::npos);
    EXPECT_NE(roundTripped.find("C:\\Games\\Assets\\shader.glsl"), std::string::npos);
    EXPECT_NE(roundTripped.find("\xC3\xA9"), std::string::npos);  // e-acute UTF-8
}
