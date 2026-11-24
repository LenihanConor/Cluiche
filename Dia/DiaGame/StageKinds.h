#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia
{
	namespace Game
	{
		// Stage kinds recognised by the stage catalogue. Order is the group order
		// (showcase, tool, test). Must match Dia/DiaGame/stage_kinds.json.
		enum class StageKind : uint8_t
		{
			kShowcase,
			kTool,
			kTest
		};

		static const unsigned int kStageKindCount = 3;

		using StageKindMask = uint8_t;

		static const char* const kStageKindNames[kStageKindCount] = { "showcase", "tool", "test" };

		inline const char* StageKindName(StageKind kind)
		{
			return kStageKindNames[static_cast<unsigned int>(kind)];
		}

		// Exact lowercase match only. Returns false (and leaves outKind untouched) if not a kind.
		inline bool KindFromTag(Dia::Core::StringCRC tag, StageKind& outKind)
		{
			for (unsigned int i = 0; i < kStageKindCount; ++i)
			{
				if (tag == Dia::Core::StringCRC(kStageKindNames[i]))
				{
					outKind = static_cast<StageKind>(i);
					return true;
				}
			}
			return false;
		}

		constexpr StageKindMask KindBit(StageKind kind)
		{
			return static_cast<StageKindMask>(1u << static_cast<unsigned int>(kind));
		}

		constexpr bool MaskHasKind(StageKindMask mask, StageKind kind)
		{
			return (mask & KindBit(kind)) != 0;
		}
	}
}
