#ifndef DIA_DEBUG_PROTOCOL_MESSAGE_TYPES_H
#define DIA_DEBUG_PROTOCOL_MESSAGE_TYPES_H

#include <cstring>

namespace Dia
{
	namespace DebugProtocol
	{
		static constexpr int kProtocolVersion = 1;

		enum class MessageType
		{
			kHandshake,
			kSubscribe,
			kUnsubscribe,
			kCoreMetrics,
			kDataUpdate,
			kEvent,
			kCommandRequest,
			kCommandResponse,
			kError,
			kGameInfo,
			kPing,
			kPong
		};

		inline const char* MessageTypeToString(MessageType type)
		{
			switch (type)
			{
			case MessageType::kHandshake:		return "handshake";
			case MessageType::kSubscribe:		return "subscribe";
			case MessageType::kUnsubscribe:		return "unsubscribe";
			case MessageType::kCoreMetrics:		return "core_metrics";
			case MessageType::kDataUpdate:		return "data_update";
			case MessageType::kEvent:			return "event";
			case MessageType::kCommandRequest:	return "command";
			case MessageType::kCommandResponse:	return "command_response";
			case MessageType::kError:			return "error";
			case MessageType::kGameInfo:		return "game_info";
			case MessageType::kPing:			return "ping";
			case MessageType::kPong:			return "pong";
			default:							return "unknown";
			}
		}

		inline MessageType StringToMessageType(const char* str)
		{
			if (!str) return MessageType::kError;

			if (strcmp(str, "handshake") == 0)			return MessageType::kHandshake;
			if (strcmp(str, "subscribe") == 0)			return MessageType::kSubscribe;
			if (strcmp(str, "unsubscribe") == 0)		return MessageType::kUnsubscribe;
			if (strcmp(str, "core_metrics") == 0)		return MessageType::kCoreMetrics;
			if (strcmp(str, "data_update") == 0)		return MessageType::kDataUpdate;
			if (strcmp(str, "event") == 0)				return MessageType::kEvent;
			if (strcmp(str, "command_response") == 0)	return MessageType::kCommandResponse;
			if (strcmp(str, "command") == 0)				return MessageType::kCommandRequest;
			if (strcmp(str, "error") == 0)				return MessageType::kError;
			if (strcmp(str, "game_info") == 0)			return MessageType::kGameInfo;
			if (strcmp(str, "ping") == 0)				return MessageType::kPing;
			if (strcmp(str, "pong") == 0)				return MessageType::kPong;

			return MessageType::kError;
		}
	}
}

#endif // DIA_DEBUG_PROTOCOL_MESSAGE_TYPES_H
