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

		struct PUMetricsPayload
		{
			char name[64];
			float fps;
			float frameTimeMs;
		};

		struct CoreMetricsPayload
		{
			float fps;
			float frameTimeMs;
			float memoryUsedMb;
			float memoryAvailableMb;
			float uptimeSeconds;

			static const unsigned int kMaxProcessingUnits = 8;
			PUMetricsPayload puMetrics[kMaxProcessingUnits];
			unsigned int puCount;
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

		// Game identity payload sent by the server to the editor on connect.
		// const char* pointers reference jsoncpp's internal buffer
		// and are valid only within the parse callback scope.
		struct GameInfoMessage
		{
			const char* name;
			const char* build;
			int processingUnitCount;
			const char* currentPhase;
		};

		// Heartbeat: editor sends ping, server echoes the timestamp back as pong.
		struct PingMessage
		{
			uint64_t ts;
		};

		struct PongMessage
		{
			uint64_t ts;
		};
	}
}

#endif // DIA_DEBUG_PROTOCOL_MESSAGE_STRUCTS_H
