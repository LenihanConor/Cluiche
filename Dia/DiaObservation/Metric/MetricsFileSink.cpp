#include "DiaObservation/Metric/MetricsFileSink.h"

#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace Observation
	{
		namespace Metric
		{
			MetricsFileSink::MetricsFileSink(const char* metricJsonlPath,
			                                 const char* metricsFinalPath,
			                                 const char* sessionId,
			                                 int64_t     epochOffsetNs)
				: mFile(nullptr)
				, mEpochOffsetNs(epochOffsetNs)
			{
				std::memset(mFinalPath, 0, sizeof(mFinalPath));
				std::memset(mSessionId, 0, sizeof(mSessionId));

				if (metricsFinalPath)
					strncpy_s(mFinalPath, metricsFinalPath, sizeof(mFinalPath) - 1);
				if (sessionId)
					strncpy_s(mSessionId, sessionId, sizeof(mSessionId) - 1);

				if (metricJsonlPath)
					fopen_s(&mFile, metricJsonlPath, "ab");
			}

			MetricsFileSink::~MetricsFileSink()
			{
				if (mFile)
				{
					fclose(mFile);
					mFile = nullptr;
				}
			}

			void MetricsFileSink::OnSnapshot(const MetricSnapshot& snapshot)
			{
				if (!mFile)
					return;

				WriteRecord(snapshot, "metric_snapshot", mFile);
			}

			void MetricsFileSink::OnFinal(const MetricSnapshot& snapshot)
			{
				// Write to metric.jsonl as well
				if (mFile)
					WriteRecord(snapshot, "metrics_final", mFile);

				// Write standalone metrics-final.json
				if (mFinalPath[0] != '\0')
				{
					FILE* finalFile = nullptr;
					fopen_s(&finalFile, mFinalPath, "wb");
					if (finalFile)
					{
						WriteRecord(snapshot, "metrics_final", finalFile);
						fclose(finalFile);
					}
				}
			}

			void MetricsFileSink::WriteRecord(const MetricSnapshot& snapshot,
			                                   const char* recordType,
			                                   FILE* target)
			{
				int64_t tsUnixNano = static_cast<int64_t>(snapshot.timestampSteadyNs) + mEpochOffsetNs;

				// Write header
				fprintf(target,
					"{\"record_type\":\"%s\","
					"\"ts_unix_nano\":%lld,"
					"\"metrics\":[",
					recordType,
					static_cast<long long>(tsUnixNano));

				// Write metric entries
				for (unsigned int i = 0; i < snapshot.entryCount; ++i)
				{
					const MetricEntry& entry = snapshot.entries[i];
					const char* nameStr = entry.name.AsChar();

					if (i > 0)
						fputc(',', target);

					switch (entry.kind)
					{
					case MetricEntry::Kind::kCounter:
						fprintf(target,
							"{\"name\":\"%s\",\"kind\":\"counter\",\"value\":%llu}",
							nameStr ? nameStr : "",
							static_cast<unsigned long long>(entry.counterValue));
						break;

					case MetricEntry::Kind::kGauge:
						fprintf(target,
							"{\"name\":\"%s\",\"kind\":\"gauge\",\"value\":%.6g}",
							nameStr ? nameStr : "",
							entry.gaugeValue);
						break;

					case MetricEntry::Kind::kHistogram:
					{
						fprintf(target,
							"{\"name\":\"%s\",\"kind\":\"histogram\","
							"\"count\":%llu,\"sum\":%.6g,\"buckets\":[",
							nameStr ? nameStr : "",
							static_cast<unsigned long long>(entry.histCount),
							entry.histSum);

						for (unsigned int b = 0; b < entry.bucketCount; ++b)
						{
							if (b > 0) fputc(',', target);

							if (b == entry.bucketCount - 1 && entry.buckets[b].le == 0.0f)
							{
								// +Inf bucket
								fprintf(target, "{\"le\":\"+Inf\",\"count\":%llu}",
									static_cast<unsigned long long>(entry.buckets[b].count));
							}
							else
							{
								fprintf(target, "{\"le\":%.6g,\"count\":%llu}",
									static_cast<double>(entry.buckets[b].le),
									static_cast<unsigned long long>(entry.buckets[b].count));
							}
						}

						fprintf(target, "],\"p50\":%.6g,\"p95\":%.6g,\"p99\":%.6g}",
							entry.p50, entry.p95, entry.p99);
						break;
					}
					}
				}

				fprintf(target, "]}\n");
				fflush(target);
			}

		} // namespace Metric
	} // namespace Observation
} // namespace Dia
