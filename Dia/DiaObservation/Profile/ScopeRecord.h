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

		// A completed job, carried through the worker thread's own job ring (separate from the
		// scope ring so ScopeRecord does not grow). Timestamps are steady-clock ns; the sink adds
		// the same epoch offset it uses for scopes.
		struct ProfileJobRecord
		{
			uint64_t submitNs;
			uint64_t startNs;
			uint64_t endNs;
			uint32_t owner;          // subject CRC value (0 = none)
			uint32_t tag;
			uint32_t submitThread;   // Profiler::CurrentThreadId() of the submitter
			uint32_t threadId;       // filled by Profiler::RecordJob (the running thread)
			uint32_t frameNumber;    // filled by Profiler::RecordJob
			uint16_t range;
			uint16_t rangeCount;
		};
	}
} // namespace Observation
} // namespace Dia
