#include "DiaObservation/Trace/TraceFileSink.h"

#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace Observation { namespace Trace
	{
		TraceFileSink::TraceFileSink(const char* path, const char* sessionId, int64_t epochOffsetNs)
			: mFile(nullptr)
			, mEpochOffsetNs(epochOffsetNs)
		{
			std::memset(mSessionId, 0, sizeof(mSessionId));
			if (sessionId)
				strncpy_s(mSessionId, sessionId, sizeof(mSessionId) - 1);

			fopen_s(&mFile, path, "ab");
		}

		TraceFileSink::~TraceFileSink()
		{
			if (mFile)
			{
				fclose(mFile);
				mFile = nullptr;
			}
		}

		void TraceFileSink::OnSpan(const SpanRecord& record)
		{
			if (!mFile)
				return;

			int64_t startUnixNano = static_cast<int64_t>(record.startSteadyNs) + mEpochOffsetNs;
			int64_t endUnixNano = static_cast<int64_t>(record.endSteadyNs) + mEpochOffsetNs;

			const char* nameStr = record.name.AsChar();
			const char* stepStr = record.scenarioStep.AsChar();

			char line[2048];
			int len = snprintf(line, sizeof(line),
				"{\"trace_id\":\"%016llx\","
				"\"span_id\":\"%016llx\","
				"\"parent_span_id\":\"%016llx\","
				"\"name\":\"%s\","
				"\"start_unix_nano\":%lld,"
				"\"end_unix_nano\":%lld,"
				"\"thread_id\":%u,"
				"\"scenario_step\":\"%s\"}\n",
				static_cast<unsigned long long>(record.traceId),
				static_cast<unsigned long long>(record.spanId),
				static_cast<unsigned long long>(record.parentSpanId),
				nameStr ? nameStr : "",
				static_cast<long long>(startUnixNano),
				static_cast<long long>(endUnixNano),
				record.threadId,
				stepStr ? stepStr : "");

			if (len > 0)
				fwrite(line, 1, static_cast<size_t>(len), mFile);
		}
	}
} // namespace Observation
} // namespace Dia
