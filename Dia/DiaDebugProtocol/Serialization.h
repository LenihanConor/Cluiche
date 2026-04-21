#ifndef DIA_DEBUG_PROTOCOL_SERIALIZATION_H
#define DIA_DEBUG_PROTOCOL_SERIALIZATION_H

#include "DiaDebugProtocol/MessageStructs.h"
#include "DiaCore/Time/TimeAbsolute.h"

namespace Dia
{
	namespace DebugProtocol
	{
		inline uint64_t GetTimestampNow()
		{
			return static_cast<uint64_t>(Dia::Core::TimeAbsolute::GetSystemTime().AsLongLongInMicroseconds());
		}

		//----------------------------------------------------------------------
		// Serialize (for sending)
		//----------------------------------------------------------------------

		inline Json::Value Serialize(const MessageHeader& header, const Json::Value& payload)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(header.type);
			msg["timestamp"] = static_cast<Json::UInt64>(header.timestamp);
			msg["payload"] = payload;
			return msg;
		}

		inline Json::Value SerializeHandshakeRequest(int protocolVersion, const char* clientName, const char* clientVersion)
		{
			Json::Value payload;
			payload["protocol_version"] = protocolVersion;
			payload["client_name"] = clientName;
			payload["client_version"] = clientVersion;

			MessageHeader header;
			header.type = MessageType::kHandshake;
			header.timestamp = GetTimestampNow();
			return Serialize(header, payload);
		}

		inline Json::Value SerializeHandshakeResponse(int protocolVersion, bool accepted, const char* serverName, const char* serverVersion)
		{
			Json::Value payload;
			payload["protocol_version"] = protocolVersion;
			payload["accepted"] = accepted;
			payload["server_name"] = serverName;
			payload["server_version"] = serverVersion;

			MessageHeader header;
			header.type = MessageType::kHandshake;
			header.timestamp = GetTimestampNow();
			return Serialize(header, payload);
		}

		inline Json::Value SerializeSubscribe(const Dia::Core::StringCRC& dataType, const Json::Value& filter)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kSubscribe);
			msg["data_type"] = dataType.AsChar();
			msg["filter"] = filter;
			return msg;
		}

		inline Json::Value SerializeUnsubscribe(const Dia::Core::StringCRC& dataType)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kUnsubscribe);
			msg["data_type"] = dataType.AsChar();
			return msg;
		}

		inline Json::Value SerializeCoreMetrics(const CoreMetricsPayload& metrics)
		{
			Json::Value payload;
			payload["fps"] = metrics.fps;
			payload["frame_time_ms"] = metrics.frameTimeMs;
			payload["memory_used_mb"] = metrics.memoryUsedMb;
			payload["memory_available_mb"] = metrics.memoryAvailableMb;

			MessageHeader header;
			header.type = MessageType::kCoreMetrics;
			header.timestamp = GetTimestampNow();
			return Serialize(header, payload);
		}

		inline Json::Value SerializeDataUpdate(const Dia::Core::StringCRC& dataType, const Json::Value& payload)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kDataUpdate);
			msg["data_type"] = dataType.AsChar();
			msg["timestamp"] = static_cast<Json::UInt64>(GetTimestampNow());
			msg["payload"] = payload;
			return msg;
		}

		inline Json::Value SerializeEvent(const Dia::Core::StringCRC& eventType, const Json::Value& payload)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kEvent);
			msg["event_type"] = eventType.AsChar();
			msg["timestamp"] = static_cast<Json::UInt64>(GetTimestampNow());
			msg["payload"] = payload;
			return msg;
		}

		inline Json::Value SerializeCommandRequest(const Dia::Core::StringCRC& command, const Json::Value& payload)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kCommandRequest);
			msg["command"] = command.AsChar();
			msg["payload"] = payload;
			return msg;
		}

		inline Json::Value SerializeCommandResponse(const Dia::Core::StringCRC& command, bool success, const char* message, const Json::Value& payload)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kCommandResponse);
			msg["command"] = command.AsChar();
			msg["success"] = success;
			msg["message"] = message ? message : "";
			msg["timestamp"] = static_cast<Json::UInt64>(GetTimestampNow());
			if (!payload.isNull())
			{
				msg["payload"] = payload;
			}
			return msg;
		}

		inline Json::Value SerializeError(const char* errorCode, const char* message)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kError);
			msg["error_code"] = errorCode ? errorCode : "";
			msg["message"] = message ? message : "";
			msg["timestamp"] = static_cast<Json::UInt64>(GetTimestampNow());
			return msg;
		}

		inline Json::Value SerializeGameInfo(const char* name, const char* build, int processingUnitCount, const char* currentPhase)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kGameInfo);
			msg["name"] = name ? name : "";
			msg["build"] = build ? build : "";
			msg["processingUnitCount"] = processingUnitCount;
			msg["currentPhase"] = currentPhase ? currentPhase : "";
			msg["timestamp"] = static_cast<Json::UInt64>(GetTimestampNow());
			return msg;
		}

		inline Json::Value SerializePing(uint64_t ts)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kPing);
			msg["ts"] = static_cast<Json::UInt64>(ts);
			return msg;
		}

		inline Json::Value SerializePong(uint64_t ts)
		{
			Json::Value msg;
			msg["type"] = MessageTypeToString(MessageType::kPong);
			msg["ts"] = static_cast<Json::UInt64>(ts);
			return msg;
		}

		//----------------------------------------------------------------------
		// Deserialize (for receiving)
		//----------------------------------------------------------------------

		inline MessageType GetMessageType(const Json::Value& message)
		{
			if (!message.isMember("type") || !message["type"].isString())
			{
				return MessageType::kError;
			}
			return StringToMessageType(message["type"].asCString());
		}

		inline bool ParseHandshakeRequest(const Json::Value& message, HandshakeRequest& out)
		{
			const Json::Value& payload = message["payload"];
			if (payload.isNull()) return false;

			if (!payload.isMember("protocol_version") || !payload["protocol_version"].isInt())
				return false;

			out.protocolVersion = payload["protocol_version"].asInt();
			out.clientName = payload.isMember("client_name") ? payload["client_name"].asCString() : "";
			out.clientVersion = payload.isMember("client_version") ? payload["client_version"].asCString() : "";
			return true;
		}

		inline bool ParseHandshakeResponse(const Json::Value& message, HandshakeResponse& out)
		{
			const Json::Value& payload = message["payload"];
			if (payload.isNull()) return false;

			if (!payload.isMember("protocol_version") || !payload["protocol_version"].isInt())
				return false;
			if (!payload.isMember("accepted") || !payload["accepted"].isBool())
				return false;

			out.protocolVersion = payload["protocol_version"].asInt();
			out.accepted = payload["accepted"].asBool();
			out.serverName = payload.isMember("server_name") ? payload["server_name"].asCString() : "";
			out.serverVersion = payload.isMember("server_version") ? payload["server_version"].asCString() : "";
			return true;
		}

		inline bool ParseSubscribe(const Json::Value& message, SubscribeMessage& out)
		{
			if (!message.isMember("data_type") || !message["data_type"].isString())
				return false;

			out.dataType = message["data_type"].asCString();
			out.filter = message.get("filter", Json::Value::null);
			return true;
		}

		inline bool ParseCoreMetrics(const Json::Value& message, CoreMetricsPayload& out)
		{
			const Json::Value& payload = message["payload"];
			if (payload.isNull()) return false;

			out.fps = payload.get("fps", 0.0f).asFloat();
			out.frameTimeMs = payload.get("frame_time_ms", 0.0f).asFloat();
			out.memoryUsedMb = payload.get("memory_used_mb", 0.0f).asFloat();
			out.memoryAvailableMb = payload.get("memory_available_mb", 0.0f).asFloat();
			return true;
		}

		inline bool ParseDataUpdate(const Json::Value& message, DataUpdateMessage& out)
		{
			if (!message.isMember("data_type") || !message["data_type"].isString())
				return false;

			out.dataType = message["data_type"].asCString();
			out.payload = message.get("payload", Json::Value::null);
			return true;
		}

		inline bool ParseEvent(const Json::Value& message, EventMessage& out)
		{
			if (!message.isMember("event_type") || !message["event_type"].isString())
				return false;

			out.eventType = message["event_type"].asCString();
			out.payload = message.get("payload", Json::Value::null);
			return true;
		}

		inline bool ParseCommandRequest(const Json::Value& message, CommandRequestMessage& out)
		{
			if (!message.isMember("command") || !message["command"].isString())
				return false;

			out.command = message["command"].asCString();
			out.payload = message.get("payload", Json::Value::null);
			return true;
		}

		inline bool ParseCommandResponse(const Json::Value& message, CommandResponseMessage& out)
		{
			if (!message.isMember("command") || !message["command"].isString())
				return false;

			out.command = message["command"].asCString();
			out.success = message.get("success", false).asBool();
			out.message = message.isMember("message") ? message["message"].asCString() : "";
			out.payload = message.get("payload", Json::Value::null);
			return true;
		}

		inline bool ParseError(const Json::Value& message, ErrorMessage& out)
		{
			out.errorCode = message.isMember("error_code") ? message["error_code"].asCString() : "";
			out.message = message.isMember("message") ? message["message"].asCString() : "";
			return true;
		}

		inline bool ParseGameInfo(const Json::Value& message, GameInfoMessage& out)
		{
			out.name = message.isMember("name") ? message["name"].asCString() : "";
			out.build = message.isMember("build") ? message["build"].asCString() : "";
			out.processingUnitCount = message.get("processingUnitCount", 0).asInt();
			out.currentPhase = message.isMember("currentPhase") ? message["currentPhase"].asCString() : "";
			return true;
		}

		inline bool ParsePing(const Json::Value& message, PingMessage& out)
		{
			if (!message.isMember("ts")) return false;
			out.ts = message["ts"].asUInt64();
			return true;
		}

		inline bool ParsePong(const Json::Value& message, PongMessage& out)
		{
			if (!message.isMember("ts")) return false;
			out.ts = message["ts"].asUInt64();
			return true;
		}

	}
}

#endif // DIA_DEBUG_PROTOCOL_SERIALIZATION_H
