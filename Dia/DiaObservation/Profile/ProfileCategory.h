#pragma once

#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		using ProfileCategory = uint32_t;

		namespace Category
		{
			constexpr ProfileCategory kNone               = 0;
			constexpr ProfileCategory kDiaApplicationFlow = 1 << 0;
			constexpr ProfileCategory kDiaGraphics        = 1 << 1;
			constexpr ProfileCategory kDiaStream          = 1 << 2;
			constexpr ProfileCategory kDiaAssetRuntime    = 1 << 3;
			constexpr ProfileCategory kDiaAnimation       = 1 << 4;
			constexpr ProfileCategory kdiaentitytemplate          = 1 << 5;
			constexpr ProfileCategory kDiaScene           = 1 << 6;
			constexpr ProfileCategory kAll                = ~0u;
		}
	}
} // namespace Observation
} // namespace Dia
