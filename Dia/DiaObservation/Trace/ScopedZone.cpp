#include "DiaObservation/Trace/ScopedZone.h"
#include "DiaObservation/Trace/Tracer.h"

#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Dia
{
	namespace Observation { namespace Trace
	{
		ScopedZone::ScopedZone(const Dia::Core::StringCRC& name, TraceCategory category)
		{
			mRecord = SpanRecord{};

			if (!Tracer::Instance().IsStarted())
				return;

			mRecord.name     = name;
			mRecord.category = category;
			mRecord.startSteadyNs = static_cast<uint64_t>(
				std::chrono::steady_clock::now().time_since_epoch().count());
#ifdef _WIN32
			mRecord.threadId = static_cast<uint32_t>(GetCurrentThreadId());
#else
			mRecord.threadId = 0;
#endif

			Tracer::Instance().OnSpanOpen(mRecord);
		}

		ScopedZone::~ScopedZone()
		{
			if (!Tracer::Instance().IsStarted())
				return;

			if (mRecord.spanId == 0)
				return;

			mRecord.endSteadyNs = static_cast<uint64_t>(
				std::chrono::steady_clock::now().time_since_epoch().count());

			Tracer::Instance().OnSpanClose(mRecord);
		}
	}
} // namespace Observation
} // namespace Dia
