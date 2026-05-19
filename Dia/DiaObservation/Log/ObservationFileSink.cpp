#include "DiaObservation/Log/ObservationFileSink.h"
#include "DiaObservation/Log/LogLevel.h"
#include "DiaObservation/JsonEscape.h"

#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace Observation { namespace Log
	{
		static int SeverityNumberFromLevel(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::kTrace:   return 1;
			case LogLevel::kDebug:   return 5;
			case LogLevel::kInfo:    return 9;
			case LogLevel::kWarning: return 13;
			case LogLevel::kError:   return 17;
			default:                 return 0;
			}
		}

		ObservationFileSink::ObservationFileSink(const char* filePath, const char* sessionId, int64_t epochOffsetNs)
			: mFile(nullptr)
			, mEpochOffsetNs(epochOffsetNs)
		{
			std::memset(mSessionId, 0, sizeof(mSessionId));
			if (sessionId)
				strncpy_s(mSessionId, sessionId, sizeof(mSessionId) - 1);

			fopen_s(&mFile, filePath, "wb");
		}

		ObservationFileSink::~ObservationFileSink()
		{
			if (mFile)
			{
				fclose(mFile);
				mFile = nullptr;
			}
		}

		void ObservationFileSink::OnLogEntry(const LogEntry& entry)
		{
			if (!mFile)
				return;

			int64_t tsUnixNano = static_cast<int64_t>(entry.timestampNs) + mEpochOffsetNs;

			const char* levelStr = LogLevelToString(entry.level);
			int severityNumber = SeverityNumberFromLevel(entry.level);

			char escapedMsg[2048];
			Dia::Observation::EscapeJsonString(entry.message, escapedMsg, sizeof(escapedMsg));

			const char* channelStr = entry.channel.AsChar();
			const char* stepStr = entry.scenarioStep.AsChar();

			char line[4096];
			int len = snprintf(line, sizeof(line),
				"{\"schema_version\":\"1.0\","
				"\"ts_unix_nano\":%lld,"
				"\"session_id\":\"%s\","
				"\"level\":\"%s\","
				"\"severity_number\":%d,"
				"\"channel\":\"%s\","
				"\"scenario_step\":\"%s\","
				"\"thread_id\":%u,"
				"\"msg\":\"%s\"}\n",
				static_cast<long long>(tsUnixNano),
				mSessionId,
				levelStr,
				severityNumber,
				channelStr ? channelStr : "",
				stepStr ? stepStr : "",
				entry.threadId,
				escapedMsg);

			if (len > 0)
				fwrite(line, 1, static_cast<size_t>(len), mFile);
		}
	}
} // namespace Observation
} // namespace Dia
