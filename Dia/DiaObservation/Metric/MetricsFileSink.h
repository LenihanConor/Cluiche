#pragma once

#include <DiaObservation/Metric/IMetricSink.h>

#include <cstdio>
#include <cstdint>

namespace Dia
{
	namespace Observation
	{
		namespace Metric
		{
			class MetricsFileSink : public IMetricSink
			{
			public:
				MetricsFileSink(const char* metricJsonlPath,
				                const char* metricsFinalPath,
				                const char* sessionId,
				                int64_t     epochOffsetNs);
				~MetricsFileSink() override;

				void OnSnapshot(const MetricSnapshot& snapshot) override;
				void OnFinal(const MetricSnapshot& snapshot) override;

				bool IsOpen() const { return mFile != nullptr; }

			private:
				void WriteRecord(const MetricSnapshot& snapshot, const char* recordType, FILE* target);

				FILE*   mFile;
				char    mFinalPath[512];
				char    mSessionId[32];
				int64_t mEpochOffsetNs;
			};

		} // namespace Metric
	} // namespace Observation
} // namespace Dia
