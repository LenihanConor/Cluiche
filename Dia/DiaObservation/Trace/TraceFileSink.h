#pragma once

#include <DiaObservation/Trace/SpanRecord.h>
#include <cstdio>
#include <cstdint>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		class TraceFileSink
		{
		public:
			TraceFileSink(const char* path, const char* sessionId, int64_t epochOffsetNs);
			~TraceFileSink();

			void OnSpan(const SpanRecord& record);
			bool IsOpen() const { return mFile != nullptr; }

		private:
			FILE*   mFile;
			char    mSessionId[32];
			int64_t mEpochOffsetNs;
		};
	}
} // namespace Observation
} // namespace Dia
