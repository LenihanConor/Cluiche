#pragma once

#include <DiaObservation/Trace/SpanRecord.h>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		class ITraceSink
		{
		public:
			virtual ~ITraceSink() = default;
			virtual void OnSpan(const SpanRecord& span) = 0;
		};
	}
} // namespace Observation
} // namespace Dia
