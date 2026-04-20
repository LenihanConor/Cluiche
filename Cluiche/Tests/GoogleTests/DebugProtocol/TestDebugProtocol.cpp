#include <gtest/gtest.h>

#include "DiaDebugProtocol/DiaDebugProtocol.h"
#include "DiaApplication/DebugDataTypes.h"

using namespace Dia::DebugProtocol;

// ============================================================================
// MessageType String Conversion
// ============================================================================

TEST(DebugProtocolMessageType, MessageTypeToString_AllTypes)
{
	EXPECT_STREQ(MessageTypeToString(MessageType::kHandshake), "handshake");
	EXPECT_STREQ(MessageTypeToString(MessageType::kSubscribe), "subscribe");
	EXPECT_STREQ(MessageTypeToString(MessageType::kUnsubscribe), "unsubscribe");
	EXPECT_STREQ(MessageTypeToString(MessageType::kCoreMetrics), "core_metrics");
	EXPECT_STREQ(MessageTypeToString(MessageType::kDataUpdate), "data_update");
	EXPECT_STREQ(MessageTypeToString(MessageType::kEvent), "event");
	EXPECT_STREQ(MessageTypeToString(MessageType::kCommandRequest), "command");
	EXPECT_STREQ(MessageTypeToString(MessageType::kCommandResponse), "command_response");
	EXPECT_STREQ(MessageTypeToString(MessageType::kError), "error");
}

TEST(DebugProtocolMessageType, StringToMessageType_AllTypes)
{
	EXPECT_EQ(StringToMessageType("handshake"), MessageType::kHandshake);
	EXPECT_EQ(StringToMessageType("subscribe"), MessageType::kSubscribe);
	EXPECT_EQ(StringToMessageType("unsubscribe"), MessageType::kUnsubscribe);
	EXPECT_EQ(StringToMessageType("core_metrics"), MessageType::kCoreMetrics);
	EXPECT_EQ(StringToMessageType("data_update"), MessageType::kDataUpdate);
	EXPECT_EQ(StringToMessageType("event"), MessageType::kEvent);
	EXPECT_EQ(StringToMessageType("command"), MessageType::kCommandRequest);
	EXPECT_EQ(StringToMessageType("command_response"), MessageType::kCommandResponse);
	EXPECT_EQ(StringToMessageType("error"), MessageType::kError);
}

TEST(DebugProtocolMessageType, StringToMessageType_UnknownReturnsError)
{
	EXPECT_EQ(StringToMessageType("garbage"), MessageType::kError);
	EXPECT_EQ(StringToMessageType(""), MessageType::kError);
	EXPECT_EQ(StringToMessageType(nullptr), MessageType::kError);
}

TEST(DebugProtocolMessageType, RoundTrip_AllTypes)
{
	MessageType types[] = {
		MessageType::kHandshake,
		MessageType::kSubscribe,
		MessageType::kUnsubscribe,
		MessageType::kCoreMetrics,
		MessageType::kDataUpdate,
		MessageType::kEvent,
		MessageType::kCommandRequest,
		MessageType::kCommandResponse,
		MessageType::kError
	};

	for (auto type : types)
	{
		const char* str = MessageTypeToString(type);
		EXPECT_EQ(StringToMessageType(str), type);
	}
}

// ============================================================================
// Handshake Serialization
// ============================================================================

TEST(DebugProtocolSerialization, SerializeHandshakeRequest_FieldsCorrect)
{
	Json::Value msg = SerializeHandshakeRequest(1, "TestEditor", "1.0.0");

	EXPECT_STREQ(msg["type"].asCString(), "handshake");
	EXPECT_TRUE(msg.isMember("timestamp"));

	const Json::Value& payload = msg["payload"];
	EXPECT_EQ(payload["protocol_version"].asInt(), 1);
	EXPECT_STREQ(payload["client_name"].asCString(), "TestEditor");
	EXPECT_STREQ(payload["client_version"].asCString(), "1.0.0");
}

TEST(DebugProtocolSerialization, ParseHandshakeRequest_RoundTrip)
{
	Json::Value msg = SerializeHandshakeRequest(1, "TestEditor", "2.0.0");

	HandshakeRequest parsed;
	EXPECT_TRUE(ParseHandshakeRequest(msg, parsed));
	EXPECT_EQ(parsed.protocolVersion, 1);
	EXPECT_STREQ(parsed.clientName, "TestEditor");
	EXPECT_STREQ(parsed.clientVersion, "2.0.0");
}

TEST(DebugProtocolSerialization, SerializeHandshakeResponse_FieldsCorrect)
{
	Json::Value msg = SerializeHandshakeResponse(1, true, "DiaDebugServer", "1.0.0");

	EXPECT_STREQ(msg["type"].asCString(), "handshake");

	const Json::Value& payload = msg["payload"];
	EXPECT_EQ(payload["protocol_version"].asInt(), 1);
	EXPECT_TRUE(payload["accepted"].asBool());
	EXPECT_STREQ(payload["server_name"].asCString(), "DiaDebugServer");
	EXPECT_STREQ(payload["server_version"].asCString(), "1.0.0");
}

TEST(DebugProtocolSerialization, ParseHandshakeResponse_RoundTrip)
{
	Json::Value msg = SerializeHandshakeResponse(1, false, "DiaDebugServer", "1.0.0");

	HandshakeResponse parsed;
	EXPECT_TRUE(ParseHandshakeResponse(msg, parsed));
	EXPECT_EQ(parsed.protocolVersion, 1);
	EXPECT_FALSE(parsed.accepted);
	EXPECT_STREQ(parsed.serverName, "DiaDebugServer");
	EXPECT_STREQ(parsed.serverVersion, "1.0.0");
}

// ============================================================================
// Subscribe/Unsubscribe Serialization
// ============================================================================

TEST(DebugProtocolSerialization, SerializeSubscribe_FieldsCorrect)
{
	Dia::Core::StringCRC dataType("processing_unit_state");
	Json::Value filter;
	filter["pu_id"] = "MainProcessingUnit";

	Json::Value msg = SerializeSubscribe(dataType, filter);

	EXPECT_STREQ(msg["type"].asCString(), "subscribe");
	EXPECT_STREQ(msg["data_type"].asCString(), "processing_unit_state");
	EXPECT_STREQ(msg["filter"]["pu_id"].asCString(), "MainProcessingUnit");
}

TEST(DebugProtocolSerialization, ParseSubscribe_RoundTrip)
{
	Dia::Core::StringCRC dataType("module_state");
	Json::Value filter;
	Json::Value msg = SerializeSubscribe(dataType, filter);

	SubscribeMessage parsed;
	EXPECT_TRUE(ParseSubscribe(msg, parsed));
	EXPECT_EQ(parsed.dataType, Dia::Core::StringCRC("module_state"));
}

TEST(DebugProtocolSerialization, SerializeUnsubscribe_FieldsCorrect)
{
	Dia::Core::StringCRC dataType("phase_transition");
	Json::Value msg = SerializeUnsubscribe(dataType);

	EXPECT_STREQ(msg["type"].asCString(), "unsubscribe");
	EXPECT_STREQ(msg["data_type"].asCString(), "phase_transition");
}

// ============================================================================
// Core Metrics Serialization
// ============================================================================

TEST(DebugProtocolSerialization, SerializeCoreMetrics_FieldsCorrect)
{
	CoreMetricsPayload metrics;
	metrics.fps = 60.0f;
	metrics.frameTimeMs = 16.67f;
	metrics.memoryUsedMb = 512.0f;
	metrics.memoryAvailableMb = 2048.0f;

	Json::Value msg = SerializeCoreMetrics(metrics);

	EXPECT_STREQ(msg["type"].asCString(), "core_metrics");
	EXPECT_TRUE(msg.isMember("timestamp"));

	const Json::Value& payload = msg["payload"];
	EXPECT_FLOAT_EQ(payload["fps"].asFloat(), 60.0f);
	EXPECT_FLOAT_EQ(payload["frame_time_ms"].asFloat(), 16.67f);
	EXPECT_FLOAT_EQ(payload["memory_used_mb"].asFloat(), 512.0f);
	EXPECT_FLOAT_EQ(payload["memory_available_mb"].asFloat(), 2048.0f);
}

TEST(DebugProtocolSerialization, ParseCoreMetrics_RoundTrip)
{
	CoreMetricsPayload metrics;
	metrics.fps = 30.0f;
	metrics.frameTimeMs = 33.33f;
	metrics.memoryUsedMb = 256.0f;
	metrics.memoryAvailableMb = 1024.0f;

	Json::Value msg = SerializeCoreMetrics(metrics);

	CoreMetricsPayload parsed;
	EXPECT_TRUE(ParseCoreMetrics(msg, parsed));
	EXPECT_FLOAT_EQ(parsed.fps, 30.0f);
	EXPECT_FLOAT_EQ(parsed.frameTimeMs, 33.33f);
	EXPECT_FLOAT_EQ(parsed.memoryUsedMb, 256.0f);
	EXPECT_FLOAT_EQ(parsed.memoryAvailableMb, 1024.0f);
}

// ============================================================================
// Data Update Serialization
// ============================================================================

TEST(DebugProtocolSerialization, SerializeDataUpdate_FieldsCorrect)
{
	Dia::Core::StringCRC dataType("processing_unit_state");
	Json::Value payload;
	payload["pu_id"] = "MainProcessingUnit";
	payload["current_phase"] = "UpdatePhase";

	Json::Value msg = SerializeDataUpdate(dataType, payload);

	EXPECT_STREQ(msg["type"].asCString(), "data_update");
	EXPECT_STREQ(msg["data_type"].asCString(), "processing_unit_state");
	EXPECT_TRUE(msg.isMember("timestamp"));
	EXPECT_STREQ(msg["payload"]["pu_id"].asCString(), "MainProcessingUnit");
}

TEST(DebugProtocolSerialization, ParseDataUpdate_RoundTrip)
{
	Dia::Core::StringCRC dataType("module_state");
	Json::Value payload;
	payload["name"] = "RenderModule";

	Json::Value msg = SerializeDataUpdate(dataType, payload);

	DataUpdateMessage parsed;
	EXPECT_TRUE(ParseDataUpdate(msg, parsed));
	EXPECT_EQ(parsed.dataType, Dia::Core::StringCRC("module_state"));
	EXPECT_STREQ(parsed.payload["name"].asCString(), "RenderModule");
}

// ============================================================================
// Event Serialization
// ============================================================================

TEST(DebugProtocolSerialization, SerializeEvent_FieldsCorrect)
{
	Dia::Core::StringCRC eventType("phase_transition");
	Json::Value payload;
	payload["from_phase"] = "UpdatePhase";
	payload["to_phase"] = "RenderPhase";

	Json::Value msg = SerializeEvent(eventType, payload);

	EXPECT_STREQ(msg["type"].asCString(), "event");
	EXPECT_STREQ(msg["event_type"].asCString(), "phase_transition");
	EXPECT_TRUE(msg.isMember("timestamp"));
	EXPECT_STREQ(msg["payload"]["from_phase"].asCString(), "UpdatePhase");
	EXPECT_STREQ(msg["payload"]["to_phase"].asCString(), "RenderPhase");
}

TEST(DebugProtocolSerialization, ParseEvent_RoundTrip)
{
	Dia::Core::StringCRC eventType("phase_transition");
	Json::Value payload;
	payload["from_phase"] = "InitPhase";
	payload["to_phase"] = "UpdatePhase";

	Json::Value msg = SerializeEvent(eventType, payload);

	EventMessage parsed;
	EXPECT_TRUE(ParseEvent(msg, parsed));
	EXPECT_EQ(parsed.eventType, Dia::Core::StringCRC("phase_transition"));
	EXPECT_STREQ(parsed.payload["from_phase"].asCString(), "InitPhase");
}

// ============================================================================
// Command Serialization
// ============================================================================

TEST(DebugProtocolSerialization, SerializeCommandRequest_FieldsCorrect)
{
	Dia::Core::StringCRC command("hot_reload");
	Json::Value payload;
	payload["manifest_path"] = "C:/path/to/manifest.diaapp";

	Json::Value msg = SerializeCommandRequest(command, payload);

	EXPECT_STREQ(msg["type"].asCString(), "command");
	EXPECT_STREQ(msg["command"].asCString(), "hot_reload");
	EXPECT_STREQ(msg["payload"]["manifest_path"].asCString(), "C:/path/to/manifest.diaapp");
}

TEST(DebugProtocolSerialization, ParseCommandRequest_RoundTrip)
{
	Dia::Core::StringCRC command("validate_manifest");
	Json::Value payload;
	payload["path"] = "test.diaapp";

	Json::Value msg = SerializeCommandRequest(command, payload);

	CommandRequestMessage parsed;
	EXPECT_TRUE(ParseCommandRequest(msg, parsed));
	EXPECT_EQ(parsed.command, Dia::Core::StringCRC("validate_manifest"));
	EXPECT_STREQ(parsed.payload["path"].asCString(), "test.diaapp");
}

TEST(DebugProtocolSerialization, SerializeCommandResponse_Success)
{
	Dia::Core::StringCRC command("hot_reload");
	Json::Value payload;

	Json::Value msg = SerializeCommandResponse(command, true, "Reload completed", payload);

	EXPECT_STREQ(msg["type"].asCString(), "command_response");
	EXPECT_STREQ(msg["command"].asCString(), "hot_reload");
	EXPECT_TRUE(msg["success"].asBool());
	EXPECT_STREQ(msg["message"].asCString(), "Reload completed");
	EXPECT_TRUE(msg.isMember("timestamp"));
}

TEST(DebugProtocolSerialization, SerializeCommandResponse_Failure)
{
	Dia::Core::StringCRC command("bad_command");
	Json::Value resultPayload;
	resultPayload["detail"] = "Not found";

	Json::Value msg = SerializeCommandResponse(command, false, "Command failed", resultPayload);

	EXPECT_FALSE(msg["success"].asBool());
	EXPECT_STREQ(msg["message"].asCString(), "Command failed");
	EXPECT_STREQ(msg["payload"]["detail"].asCString(), "Not found");
}

TEST(DebugProtocolSerialization, ParseCommandResponse_RoundTrip)
{
	Dia::Core::StringCRC command("get_state");
	Json::Value resultPayload;
	resultPayload["state"] = "running";

	Json::Value msg = SerializeCommandResponse(command, true, "OK", resultPayload);

	CommandResponseMessage parsed;
	EXPECT_TRUE(ParseCommandResponse(msg, parsed));
	EXPECT_EQ(parsed.command, Dia::Core::StringCRC("get_state"));
	EXPECT_TRUE(parsed.success);
	EXPECT_STREQ(parsed.message, "OK");
	EXPECT_STREQ(parsed.payload["state"].asCString(), "running");
}

// ============================================================================
// Error Serialization
// ============================================================================

TEST(DebugProtocolSerialization, SerializeError_FieldsCorrect)
{
	Json::Value msg = SerializeError("INVALID_COMMAND", "Unknown command: 'bad_cmd'");

	EXPECT_STREQ(msg["type"].asCString(), "error");
	EXPECT_STREQ(msg["error_code"].asCString(), "INVALID_COMMAND");
	EXPECT_STREQ(msg["message"].asCString(), "Unknown command: 'bad_cmd'");
	EXPECT_TRUE(msg.isMember("timestamp"));
}

TEST(DebugProtocolSerialization, SerializeError_NullFieldsHandledGracefully)
{
	Json::Value msg = SerializeError(nullptr, nullptr);

	EXPECT_STREQ(msg["error_code"].asCString(), "");
	EXPECT_STREQ(msg["message"].asCString(), "");
}

TEST(DebugProtocolSerialization, ParseError_RoundTrip)
{
	Json::Value msg = SerializeError("PARSE_ERROR", "Invalid JSON");

	ErrorMessage parsed;
	EXPECT_TRUE(ParseError(msg, parsed));
	EXPECT_STREQ(parsed.errorCode, "PARSE_ERROR");
	EXPECT_STREQ(parsed.message, "Invalid JSON");
}

// ============================================================================
// GetMessageType
// ============================================================================

TEST(DebugProtocolSerialization, GetMessageType_ExtractsCorrectType)
{
	Json::Value msg = SerializeCoreMetrics({60.0f, 16.67f, 512.0f, 2048.0f});
	EXPECT_EQ(GetMessageType(msg), MessageType::kCoreMetrics);

	Json::Value errMsg = SerializeError("CODE", "msg");
	EXPECT_EQ(GetMessageType(errMsg), MessageType::kError);

	Json::Value handshake = SerializeHandshakeRequest(1, "Editor", "1.0");
	EXPECT_EQ(GetMessageType(handshake), MessageType::kHandshake);
}

TEST(DebugProtocolSerialization, GetMessageType_MissingTypeFieldReturnsError)
{
	Json::Value empty;
	EXPECT_EQ(GetMessageType(empty), MessageType::kError);

	Json::Value noType;
	noType["payload"] = "data";
	EXPECT_EQ(GetMessageType(noType), MessageType::kError);
}

// ============================================================================
// Parse Failure Cases
// ============================================================================

TEST(DebugProtocolParsing, ParseHandshakeRequest_MissingPayloadReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "handshake";

	HandshakeRequest parsed;
	EXPECT_FALSE(ParseHandshakeRequest(msg, parsed));
}

TEST(DebugProtocolParsing, ParseHandshakeRequest_MissingVersionReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "handshake";
	msg["payload"]["client_name"] = "Editor";

	HandshakeRequest parsed;
	EXPECT_FALSE(ParseHandshakeRequest(msg, parsed));
}

TEST(DebugProtocolParsing, ParseHandshakeResponse_MissingAcceptedReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "handshake";
	msg["payload"]["protocol_version"] = 1;

	HandshakeResponse parsed;
	EXPECT_FALSE(ParseHandshakeResponse(msg, parsed));
}

TEST(DebugProtocolParsing, ParseSubscribe_MissingDataTypeReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "subscribe";

	SubscribeMessage parsed;
	EXPECT_FALSE(ParseSubscribe(msg, parsed));
}

TEST(DebugProtocolParsing, ParseCoreMetrics_MissingPayloadReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "core_metrics";

	CoreMetricsPayload parsed;
	EXPECT_FALSE(ParseCoreMetrics(msg, parsed));
}

TEST(DebugProtocolParsing, ParseCommandRequest_MissingCommandReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "command";
	msg["payload"]["arg"] = "value";

	CommandRequestMessage parsed;
	EXPECT_FALSE(ParseCommandRequest(msg, parsed));
}

TEST(DebugProtocolParsing, ParseCommandResponse_MissingCommandReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "command_response";
	msg["success"] = true;

	CommandResponseMessage parsed;
	EXPECT_FALSE(ParseCommandResponse(msg, parsed));
}

TEST(DebugProtocolParsing, ParseDataUpdate_MissingDataTypeReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "data_update";
	msg["payload"]["data"] = "value";

	DataUpdateMessage parsed;
	EXPECT_FALSE(ParseDataUpdate(msg, parsed));
}

TEST(DebugProtocolParsing, ParseEvent_MissingEventTypeReturnsFalse)
{
	Json::Value msg;
	msg["type"] = "event";
	msg["payload"]["data"] = "value";

	EventMessage parsed;
	EXPECT_FALSE(ParseEvent(msg, parsed));
}

// ============================================================================
// Data Type Constants
// ============================================================================

TEST(DebugProtocolDataTypes, CoreDataTypeConstant)
{
	EXPECT_EQ(DataType::kCoreMetrics, Dia::Core::StringCRC("core_metrics"));
}

TEST(DebugProtocolDataTypes, ApplicationDataTypeConstants)
{
	EXPECT_EQ(Dia::Application::DebugDataType::kProcessingUnitState, Dia::Core::StringCRC("processing_unit_state"));
	EXPECT_EQ(Dia::Application::DebugDataType::kPhaseTransition, Dia::Core::StringCRC("phase_transition"));
	EXPECT_EQ(Dia::Application::DebugDataType::kModuleState, Dia::Core::StringCRC("module_state"));
	EXPECT_EQ(Dia::Application::DebugDataType::kMessageBus, Dia::Core::StringCRC("message_bus"));
	EXPECT_EQ(Dia::Application::DebugDataType::kPerformanceBreakdown, Dia::Core::StringCRC("performance_breakdown"));
}

// ============================================================================
// Protocol Version
// ============================================================================

TEST(DebugProtocolVersion, VersionIsOne)
{
	EXPECT_EQ(kProtocolVersion, 1);
}

TEST(DebugProtocolVersion, HandshakeCarriesVersion)
{
	Json::Value msg = SerializeHandshakeRequest(kProtocolVersion, "Editor", "1.0");

	HandshakeRequest parsed;
	EXPECT_TRUE(ParseHandshakeRequest(msg, parsed));
	EXPECT_EQ(parsed.protocolVersion, kProtocolVersion);
}
