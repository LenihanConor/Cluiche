#pragma once

#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		using TraceCategory = uint32_t;

		namespace Category
		{
			constexpr TraceCategory kNone               = 0;
			constexpr TraceCategory kDiaApplicationFlow = 1u << 0;
			constexpr TraceCategory kDiaGraphics        = 1u << 1;
			constexpr TraceCategory kDiaStream          = 1u << 2;
			constexpr TraceCategory kDiaAssetRuntime    = 1u << 3;
			constexpr TraceCategory kDiaAnimation       = 1u << 4;
			constexpr TraceCategory kDiaEntity          = 1u << 5;
			constexpr TraceCategory kDiaScene           = 1u << 6;
			constexpr TraceCategory kAll                = ~0u;
		}
	}
} // namespace Observation
} // namespace Dia
