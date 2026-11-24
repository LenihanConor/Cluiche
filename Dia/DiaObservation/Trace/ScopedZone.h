#pragma once

#include <DiaObservation/Trace/SpanRecord.h>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		struct TraceLink;

		class ScopedZone
		{
		public:
			explicit ScopedZone(const Dia::Core::StringCRC& name, TraceCategory category);
			ScopedZone(const Dia::Core::StringCRC& name, TraceCategory category, const TraceLink& link);
			~ScopedZone();

			ScopedZone(const ScopedZone&) = delete;
			ScopedZone& operator=(const ScopedZone&) = delete;

		private:
			SpanRecord mRecord;
		};
	}
} // namespace Observation
} // namespace Dia
