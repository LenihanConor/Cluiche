#pragma once

#include <cstdint>

namespace Dia
{
	namespace Threading
	{
		// Explicit attribution, e.g. a deferred-step channel.
		struct JobTag
		{
			uint32_t owner      = 0;   // CRC value of the owner (module instance id or channel id)
			uint32_t tag        = 0;   // free-form, e.g. kick sim key
			uint16_t range      = 0;
			uint16_t rangeCount = 0;
		};

		struct JobContext
		{
			JobTag   attribution;
			uint64_t traceParent  = 0;       // span id of the submitting scope (0 = none)
			uint32_t submitThread = 0;
			void*    inbox        = nullptr; // opaque to DiaThreading; the observer's per-PU inbox
		};

		struct JobTimingRecord             // 40 bytes in memory
		{
			uint64_t submitNs;
			uint64_t startNs;
			uint64_t endNs;
			uint32_t owner;
			uint32_t tag;
			uint16_t range;
			uint16_t rangeCount;
			uint16_t worker;
			uint16_t flags;                // e.g. ran inline (TrySubmit fallback)
		};
		static_assert(sizeof(JobTimingRecord) == 40, "JobTimingRecord must be 40 bytes");
	}
}
