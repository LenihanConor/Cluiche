#include "DiaObservation/Log/ObservationFileSink.h"
#include "DiaObservation/Log/LogLevel.h"
#include "DiaObservation/JsonEscape.h"

#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace Observation { namespace Log
	{
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

			char escapedMsg[2048];
			Dia::Observation::EscapeJsonString(entry.message, escapedMsg, sizeof(escapedMsg));

			const char* channelStr = entry.channel.AsChar();

			char line[4096];
			int len = snprintf(line, sizeof(line),
				"{\"ts_unix_nano\":%lld,"
				"\"level\":\"%s\","
				"\"channel\":\"%s\","
				"\"thread_id\":%u,"
				"\"msg\":\"%s\"}\n",
				static_cast<long long>(tsUnixNano),
				levelStr,
				channelStr ? channelStr : "",
				entry.threadId,
				escapedMsg);

			if (len > 0)
				fwrite(line, 1, static_cast<size_t>(len), mFile);
		}
	}
} // namespace Observation
} // namespace Dia
