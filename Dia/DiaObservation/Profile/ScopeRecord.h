#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Profile/ProfileCategory.h>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		struct ScopeRecord
		{
			Dia::Core::StringCRC name;
			ProfileCategory      category;
			uint64_t             scopeId;
			uint64_t             parentScopeId;   // 0 if root scope
			uint64_t             startUnixNano;
			uint64_t             durationNs;
			uint32_t             frameNumber;
			uint32_t             threadId;
		};
	}
} // namespace Observation
} // namespace Dia
