#ifndef DIA_DEBUG_SERVER_SIM_HOSTED_TYPES_H
#define DIA_DEBUG_SERVER_SIM_HOSTED_TYPES_H

#include <cstdint>
#include <type_traits>

namespace Dia
{
	namespace DebugServer
	{
		// Max JSON payload carried by a sim-hosted request or reply (fixed-size, no heap).
		static constexpr uint32_t kSimPayloadBytes = 16384;

		enum class SimKind : uint32_t
		{
			Query = 0,
			Command = 1
		};

		enum class SimReplyStatus : uint32_t
		{
			Ok = 0,
			HandlerFailed = 1,
			TooLarge = 2,
			UnknownName = 3
		};

		// Main -> Sim. 'kind' holds a SimKind value.
		struct SimRequestEvent
		{
			uint32_t requestId;
			uint32_t nameCrc;
			uint32_t kind;
			char json[kSimPayloadBytes];
		};

		// Sim -> Main. 'status' holds a SimReplyStatus value.
		struct SimReplyEvent
		{
			uint32_t requestId;
			uint32_t status;
			char json[kSimPayloadBytes];
		};

		static_assert(std::is_trivially_copyable_v<SimRequestEvent>, "SimRequestEvent must be trivially copyable");
		static_assert(std::is_trivially_copyable_v<SimReplyEvent>, "SimReplyEvent must be trivially copyable");
	}
}

#endif
