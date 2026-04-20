#ifndef DIA_DEBUG_PROTOCOL_MESSAGE_STRUCTS_H
#define DIA_DEBUG_PROTOCOL_MESSAGE_STRUCTS_H

#include "DiaDebugProtocol/MessageTypes.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Json/external/json/json.h"

#include <cstdint>

namespace Dia
{
	namespace DebugProtocol
	{
		struct MessageHeader
		{
			MessageType type;
			uint64_t timestamp;
		};

		struct SubscribeMessage
		{
			Dia::Core::StringCRC dataType;
			Json::Value filter;
		};

		struct CoreMetricsPayload
		{
			float fps;
			float frameTimeMs;
			float memoryUsedMb;
			float memoryAvailableMb;
		};

		struct DataUpdateMessage
		{
			Dia::Core::StringCRC dataType;
			Json::Value payload;
		};

		struct EventMessage
		{
			Dia::Core::StringCRC eventType;
			Json::Value payload;
		};

		// const char* pointers reference jsoncpp's internal buffer
		// and are valid only within the parse callback scope.
		struct CommandRequestMessage
		{
			Dia::Core::StringCRC command;
			Json::Value payload;
		};

		// const char* pointers reference jsoncpp's internal buffer
		// and are valid only within the parse callback scope.
		struct CommandResponseMessage
		{
			Dia::Core::StringCRC command;
			bool success;
			const char* message;
			Json::Value payload;
		};

		// const char* pointers reference jsoncpp's internal buffer
		// and are valid only within the parse callback scope.
		struct HandshakeRequest
		{
			int protocolVersion;
			const char* clientName;
			const char* clientVersion;
		};

		// const char* pointers reference jsoncpp's internal buffer
		// and are valid only within the parse callback scope.
		struct HandshakeResponse
		{
			int protocolVersion;
			bool accepted;
			const char* serverName;
			const char* serverVersion;
		};

		// const char* pointers reference jsoncpp's internal buffer
		// and are valid only within the parse callback scope.
		struct ErrorMessage
		{
			const char* errorCode;
			const char* message;
		};
	}
}

#endif // DIA_DEBUG_PROTOCOL_MESSAGE_STRUCTS_H
