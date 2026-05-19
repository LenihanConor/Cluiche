#pragma once

#include <DiaObservation/Trace/SpanRecord.h>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		class ScopedZone
		{
		public:
			explicit ScopedZone(const Dia::Core::StringCRC& name);
			~ScopedZone();

			ScopedZone(const ScopedZone&) = delete;
			ScopedZone& operator=(const ScopedZone&) = delete;

		private:
			SpanRecord mRecord;
		};
	}
} // namespace Observation
} // namespace Dia
