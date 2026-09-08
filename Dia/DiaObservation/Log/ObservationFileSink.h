#pragma once

#include <DiaObservation/Log/ISink.h>

#include <cstdint>
#include <cstdio>

namespace Dia
{
	namespace Observation { namespace Log
	{
		class ObservationFileSink : public ISink
		{
		public:
			ObservationFileSink(const char* filePath, const char* sessionId, int64_t epochOffsetNs);
			~ObservationFileSink() override;

			void OnLogEntry(const LogEntry& entry) override;
			const char* GetName() const override { return "ObservationFile"; }

			bool IsOpen() const { return mFile != nullptr; }

		private:
			FILE* mFile;
			char mSessionId[32];
			int64_t mEpochOffsetNs;
		};
	}
} // namespace Observation
} // namespace Dia
