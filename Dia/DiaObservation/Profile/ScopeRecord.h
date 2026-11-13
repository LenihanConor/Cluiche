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
			uint32_t             subject;         // CRC value of the owning subject (module/PU); 0 = unset
		};
	}
} // namespace Observation
} // namespace Dia
