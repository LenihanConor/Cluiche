#ifndef DIA_DEBUG_SERVER_SIM_HOSTED_TYPES_H
#define DIA_DEBUG_SERVER_SIM_HOSTED_TYPES_H

#include <DiaCore/CRC/StringCRC.h>

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
			Command = 1,
			Resync = 2   // control request (announcement resync); never a handler name
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

		// Max length (excluding NUL) of a sim-hosted name carried in SimHostedName.
		static constexpr uint32_t kSimHostedNameMax = 96;

		struct SimHelpStored;   // SimHelpInfo.h

		// One (name, kind, help) entry for DebugServer::ReplaceSimHosted.  'help' points at the caller's stored help
		// (~7 KB, required, copied by the call) so a 32-entry array stays small on the caller's stack.
		struct SimHostedName
		{
			char                 name[kSimHostedNameMax];
			SimKind              kind;
			const SimHelpStored* help;
		};

		// Read-only view of one sim-hosted catalogue entry (DebugServer::GetSimHostedEntry).  'help' points into the
		// DebugServer and is valid until the next Register/Unregister/ReplaceSimHosted call.
		struct SimHostedEntryView
		{
			Dia::Core::StringCRC name;
			SimKind              kind = SimKind::Query;
			const SimHelpStored* help = nullptr;
		};

		// Per-entry hash of (name, kind): FNV-1a over the name bytes, then the kind value, then an avalanche.
		// Deterministic; shared by the sim host and the DebugServer.  Set hash = uint32 wrapping sum of entry hashes.
		inline uint32_t SimNameSetEntryHash(const char* name, SimKind kind)
		{
			uint32_t h = 2166136261u;
			for (const char* p = (name != nullptr) ? name : ""; *p != '\0'; ++p)
			{
				h ^= static_cast<uint8_t>(*p);
				h *= 16777619u;
			}
			h ^= static_cast<uint32_t>(kind) + 1u;
			h *= 16777619u;
			h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
			return h;
		}

		static_assert(std::is_trivially_copyable_v<SimRequestEvent>, "SimRequestEvent must be trivially copyable");
		static_assert(std::is_trivially_copyable_v<SimReplyEvent>, "SimReplyEvent must be trivially copyable");
	}
}

#endif
