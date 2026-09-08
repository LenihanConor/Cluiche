#include "DiaDebugServer/ObservationBridge.h"

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Trace/Tracer.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/JsonEscape.h>
#include <DiaWebSocket/Server.h>

#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace DebugServer
	{
		ObservationBridge::ObservationBridge(Dia::WebSocket::Server* server)
			: mServer(server)
			, mSubscriberQuery(nullptr)
			, mEpochOffsetNs(0)
			, mActive(false)
			, mLogBatchCount(0)
			, mLogBatchElapsedSec(0.0f)
			, mTraceBatchCount(0)
			, mTraceBatchElapsedSec(0.0f)
			, mHasPendingMetric(false)
			, mMetricElapsedSec(0.0f)
		{
			std::memset(mSessionId, 0, sizeof(mSessionId));
			std::memset(mLatestMetricBuf, 0, sizeof(mLatestMetricBuf));
		}

		ObservationBridge::~ObservationBridge()
		{
			if (mActive.load(std::memory_order_acquire))
				Stop();
		}

		void ObservationBridge::Start(const char* sessionId, int64_t epochOffsetNs)
		{
			if (mActive.load(std::memory_order_acquire))
				return;

			if (sessionId)
				strncpy_s(mSessionId, sessionId, sizeof(mSessionId) - 1);
			mEpochOffsetNs = epochOffsetNs;

			Dia::Observation::Log::Logger::Instance().RegisterSink(this);
			Dia::Observation::Trace::Tracer::Instance().RegisterTraceSink(this);
			Dia::Observation::Metric::MetricRegistry::Instance().RegisterSink(this);
			Dia::Observation::Health::HealthRegistry::Instance().RegisterTransitionSink(this);

			mActive.store(true, std::memory_order_release);
		}

		void ObservationBridge::Stop()
		{
			if (!mActive.exchange(false, std::memory_order_acq_rel))
				return;

			// Flush any pending batches before unregistering sinks
			FlushLogBatch();
			FlushTraceBatch();
			FlushMetricBatch();

			Dia::Observation::Health::HealthRegistry::Instance().UnregisterTransitionSink(this);
			Dia::Observation::Metric::MetricRegistry::Instance().UnregisterSink(this);
			Dia::Observation::Trace::Tracer::Instance().UnregisterTraceSink(this);
			Dia::Observation::Log::Logger::Instance().UnregisterSink(this);
		}

		void ObservationBridge::SetSubscriberQuery(SubscriberQueryFn fn)
		{
			mSubscriberQuery = std::move(fn);
		}

		void ObservationBridge::SendToSubscribers(const char* topic, const char* buf)
		{
			if (!mServer || !mSubscriberQuery)
				return;

			Dia::Core::Containers::DynamicArrayC<int, 16> subscribers;
			mSubscriberQuery(topic, subscribers);

			for (unsigned int i = 0; i < subscribers.Size(); ++i)
				mServer->SendText(subscribers[i], buf);
		}

		void ObservationBridge::OnLogEntry(const Dia::Observation::Log::LogEntry& entry)
		{
			if (!mActive.load(std::memory_order_acquire) || !mServer)
				return;

			// Early exit if no subscribers — avoid buffering for zero subscribers
			if (!mSubscriberQuery)
				return;
			Dia::Core::Containers::DynamicArrayC<int, 16> subscribers;
			mSubscriberQuery("observation.log", subscribers);
			if (subscribers.Size() == 0)
				return;

			int64_t tsUnixNano = static_cast<int64_t>(entry.timestampNs) + mEpochOffsetNs;
			const char* channelStr = entry.channel.AsChar();
			const char* stepStr = entry.scenarioStep.AsChar();

			char escapedMsg[1024];
			Dia::Observation::EscapeJsonString(entry.message, escapedMsg, sizeof(escapedMsg));

			char buf[2048];
			snprintf(buf, sizeof(buf),
				"{\"topic\":\"observation.log\","
				"\"schema_version\":\"1.0\","
				"\"ts_unix_nano\":%lld,"
				"\"session_id\":\"%s\","
				"\"level\":\"%s\","
				"\"severity_number\":%d,"
				"\"channel\":\"%s\","
				"\"scenario_step\":\"%s\","
				"\"thread_id\":%u,"
				"\"msg\":\"%s\"}",
				static_cast<long long>(tsUnixNano),
				mSessionId,
				Dia::Observation::Log::LogLevelToString(entry.level),
				static_cast<int>(entry.level) * 4 + 1,
				channelStr ? channelStr : "",
				stepStr ? stepStr : "",
				entry.threadId,
				escapedMsg);

			{
				std::lock_guard<std::mutex> lock(mLogMutex);
				if (mLogBatchCount < kMaxLogBatch)
					strncpy_s(mLogBatch[mLogBatchCount++], sizeof(mLogBatch[0]), buf, _TRUNCATE);

				if (mLogBatchCount >= kMaxLogBatch)
					FlushLogBatchLocked();
			}
		}

		void ObservationBridge::OnSpan(const Dia::Observation::Trace::SpanRecord& span)
		{
			if (!mActive.load(std::memory_order_acquire) || !mServer)
				return;

			// Early exit if no subscribers — avoid buffering for zero subscribers
			if (!mSubscriberQuery)
				return;
			Dia::Core::Containers::DynamicArrayC<int, 16> subscribers;
			mSubscriberQuery("observation.trace", subscribers);
			if (subscribers.Size() == 0)
				return;

			int64_t startUnixNano = static_cast<int64_t>(span.startSteadyNs) + mEpochOffsetNs;
			int64_t endUnixNano = static_cast<int64_t>(span.endSteadyNs) + mEpochOffsetNs;
			const char* nameStr = span.name.AsChar();
			const char* stepStr = span.scenarioStep.AsChar();

			char buf[2048];
			snprintf(buf, sizeof(buf),
				"{\"topic\":\"observation.trace\","
				"\"schema_version\":\"1.0\","
				"\"record_type\":\"span\","
				"\"session_id\":\"%s\","
				"\"trace_id\":\"%016llx\","
				"\"span_id\":\"%016llx\","
				"\"parent_span_id\":\"%016llx\","
				"\"name\":\"%s\","
				"\"start_unix_nano\":%lld,"
				"\"end_unix_nano\":%lld,"
				"\"thread_id\":%u,"
				"\"scenario_step\":\"%s\"}",
				mSessionId,
				static_cast<unsigned long long>(span.traceId),
				static_cast<unsigned long long>(span.spanId),
				static_cast<unsigned long long>(span.parentSpanId),
				nameStr ? nameStr : "",
				static_cast<long long>(startUnixNano),
				static_cast<long long>(endUnixNano),
				span.threadId,
				stepStr ? stepStr : "");

			{
				std::lock_guard<std::mutex> lock(mTraceMutex);
				if (mTraceBatchCount < kMaxTraceBatch)
					strncpy_s(mTraceBatch[mTraceBatchCount++], sizeof(mTraceBatch[0]), buf, _TRUNCATE);

				if (mTraceBatchCount >= kMaxTraceBatch)
					FlushTraceBatchLocked();
			}
		}

		void ObservationBridge::OnSnapshot(const Dia::Observation::Metric::MetricSnapshot& snapshot)
		{
			if (!mActive.load(std::memory_order_acquire) || !mServer)
				return;

			// Early exit if no subscribers — avoid buffering for zero subscribers
			if (!mSubscriberQuery)
				return;
			Dia::Core::Containers::DynamicArrayC<int, 16> subscribers;
			mSubscriberQuery("observation.metric", subscribers);
			if (subscribers.Size() == 0)
				return;

			std::lock_guard<std::mutex> lock(mMetricMutex);

			int64_t tsUnixNano = static_cast<int64_t>(snapshot.timestampSteadyNs) + mEpochOffsetNs;

			int pos = snprintf(mLatestMetricBuf, sizeof(mLatestMetricBuf),
				"{\"topic\":\"observation.metric\","
				"\"schema_version\":\"1.0\","
				"\"record_type\":\"metric_snapshot\","
				"\"session_id\":\"%s\","
				"\"ts_unix_nano\":%lld,"
				"\"interval_ms\":%u,"
				"\"metrics\":[",
				mSessionId,
				static_cast<long long>(tsUnixNano),
				snapshot.intervalMs);

			for (unsigned int i = 0; i < snapshot.entryCount && pos < static_cast<int>(sizeof(mLatestMetricBuf)) - 256; ++i)
			{
				const Dia::Observation::Metric::MetricEntry& entry = snapshot.entries[i];
				const char* nameStr = entry.name.AsChar();

				if (i > 0) mLatestMetricBuf[pos++] = ',';

				switch (entry.kind)
				{
				case Dia::Observation::Metric::MetricEntry::Kind::kCounter:
					pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos,
						"{\"name\":\"%s\",\"kind\":\"counter\",\"value\":%llu}",
						nameStr ? nameStr : "",
						static_cast<unsigned long long>(entry.counterValue));
					break;

				case Dia::Observation::Metric::MetricEntry::Kind::kGauge:
					pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos,
						"{\"name\":\"%s\",\"kind\":\"gauge\",\"value\":%.6g}",
						nameStr ? nameStr : "",
						entry.gaugeValue);
					break;

				case Dia::Observation::Metric::MetricEntry::Kind::kHistogram:
					pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos,
						"{\"name\":\"%s\",\"kind\":\"histogram\","
						"\"count\":%llu,\"sum\":%.6g,"
						"\"p50\":%.6g,\"p95\":%.6g,\"p99\":%.6g}",
						nameStr ? nameStr : "",
						static_cast<unsigned long long>(entry.histCount),
						entry.histSum,
						entry.p50, entry.p95, entry.p99);
					break;
				}
			}

			pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos, "]}");
			mHasPendingMetric = true;
		}

		void ObservationBridge::OnFinal(const Dia::Observation::Metric::MetricSnapshot& snapshot)
		{
			if (!mActive.load(std::memory_order_acquire) || !mServer)
				return;

			if (!mSubscriberQuery)
				return;
			Dia::Core::Containers::DynamicArrayC<int, 16> subscribers;
			mSubscriberQuery("observation.metric", subscribers);
			if (subscribers.Size() == 0)
				return;

			std::lock_guard<std::mutex> lock(mMetricMutex);

			int64_t tsUnixNano = static_cast<int64_t>(snapshot.timestampSteadyNs) + mEpochOffsetNs;

			int pos = snprintf(mLatestMetricBuf, sizeof(mLatestMetricBuf),
				"{\"topic\":\"observation.metric\","
				"\"schema_version\":\"1.0\","
				"\"record_type\":\"metric_snapshot\","
				"\"session_id\":\"%s\","
				"\"ts_unix_nano\":%lld,"
				"\"interval_ms\":%u,"
				"\"metrics\":[",
				mSessionId,
				static_cast<long long>(tsUnixNano),
				snapshot.intervalMs);

			for (unsigned int i = 0; i < snapshot.entryCount && pos < static_cast<int>(sizeof(mLatestMetricBuf)) - 256; ++i)
			{
				const Dia::Observation::Metric::MetricEntry& entry = snapshot.entries[i];
				const char* nameStr = entry.name.AsChar();

				if (i > 0) mLatestMetricBuf[pos++] = ',';

				switch (entry.kind)
				{
				case Dia::Observation::Metric::MetricEntry::Kind::kCounter:
					pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos,
						"{\"name\":\"%s\",\"kind\":\"counter\",\"value\":%llu}",
						nameStr ? nameStr : "",
						static_cast<unsigned long long>(entry.counterValue));
					break;

				case Dia::Observation::Metric::MetricEntry::Kind::kGauge:
					pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos,
						"{\"name\":\"%s\",\"kind\":\"gauge\",\"value\":%.6g}",
						nameStr ? nameStr : "",
						entry.gaugeValue);
					break;

				case Dia::Observation::Metric::MetricEntry::Kind::kHistogram:
					pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos,
						"{\"name\":\"%s\",\"kind\":\"histogram\","
						"\"count\":%llu,\"sum\":%.6g,"
						"\"p50\":%.6g,\"p95\":%.6g,\"p99\":%.6g}",
						nameStr ? nameStr : "",
						static_cast<unsigned long long>(entry.histCount),
						entry.histSum,
						entry.p50, entry.p95, entry.p99);
					break;
				}
			}

			pos += snprintf(mLatestMetricBuf + pos, sizeof(mLatestMetricBuf) - pos, "]}");
			mHasPendingMetric = true;

			FlushMetricBatchLocked();
		}

		void ObservationBridge::OnTransition(
			const Dia::Observation::Health::HealthRegistry::Transition& transition)
		{
			if (!mActive.load(std::memory_order_acquire) || !mServer)
				return;

			if (!mSubscriberQuery)
				return;

			const char* reporterStr = transition.reporterName.AsChar();
			const char* reasonStr = transition.reason.AsChar();

			char escapedReason[256];
			Dia::Observation::EscapeJsonString(reasonStr ? reasonStr : "", escapedReason, sizeof(escapedReason));

			char buf[1024];
			snprintf(buf, sizeof(buf),
				"{\"topic\":\"observation.health\","
				"\"schema_version\":\"1.0\","
				"\"record_type\":\"health\","
				"\"session_id\":\"%s\","
				"\"reporter\":\"%s\","
				"\"old_status\":\"%s\","
				"\"new_status\":\"%s\","
				"\"reason\":\"%s\"}",
				mSessionId,
				reporterStr ? reporterStr : "",
				Dia::Observation::Health::HealthStatusToString(transition.oldStatus),
				Dia::Observation::Health::HealthStatusToString(transition.newStatus),
				escapedReason);

			// Health is always immediate — rare and important, no batching.
			SendToSubscribers("observation.health", buf);
		}

		//---------------------------------------------------------------------
		// Batch flush helpers
		//---------------------------------------------------------------------

		void ObservationBridge::FlushLogBatch()
		{
			std::lock_guard<std::mutex> lock(mLogMutex);
			if (mLogBatchCount == 0 || !mServer || !mSubscriberQuery)
				return;

			char arrayBuf[kMaxLogBatch * 2048 + 64];
			int pos = 0;
			int remaining = static_cast<int>(sizeof(arrayBuf));
			pos += snprintf(arrayBuf + pos, remaining - pos,
				"{\"topic\":\"observation.log_batch\",\"entries\":[");
			for (int i = 0; i < mLogBatchCount && pos < remaining - 4; ++i)
			{
				if (i > 0) arrayBuf[pos++] = ',';
				int written = snprintf(arrayBuf + pos, remaining - pos, "%s", mLogBatch[i]);
				if (written < 0 || written >= remaining - pos) break;
				pos += written;
			}
			snprintf(arrayBuf + pos, remaining - pos, "]}");

			SendToSubscribers("observation.log", arrayBuf);
			mLogBatchCount = 0;
			mLogBatchElapsedSec = 0.0f;
		}

		void ObservationBridge::FlushTraceBatch()
		{
			std::lock_guard<std::mutex> lock(mTraceMutex);
			if (mTraceBatchCount == 0 || !mServer || !mSubscriberQuery)
				return;

			char arrayBuf[kMaxTraceBatch * 2048 + 64];
			int pos = 0;
			int remaining = static_cast<int>(sizeof(arrayBuf));
			pos += snprintf(arrayBuf + pos, remaining - pos,
				"{\"topic\":\"observation.trace_batch\",\"entries\":[");
			for (int i = 0; i < mTraceBatchCount && pos < remaining - 4; ++i)
			{
				if (i > 0) arrayBuf[pos++] = ',';
				int written = snprintf(arrayBuf + pos, remaining - pos, "%s", mTraceBatch[i]);
				if (written < 0 || written >= remaining - pos) break;
				pos += written;
			}
			snprintf(arrayBuf + pos, remaining - pos, "]}");

			SendToSubscribers("observation.trace", arrayBuf);
			mTraceBatchCount = 0;
			mTraceBatchElapsedSec = 0.0f;
		}

		void ObservationBridge::FlushMetricBatch()
		{
			std::lock_guard<std::mutex> lock(mMetricMutex);
			if (!mHasPendingMetric || !mServer || !mSubscriberQuery)
				return;

			SendToSubscribers("observation.metric", mLatestMetricBuf);
			mHasPendingMetric = false;
			mMetricElapsedSec = 0.0f;
		}

		void ObservationBridge::FlushLogBatchLocked()
		{
			if (mLogBatchCount == 0 || !mServer || !mSubscriberQuery)
				return;

			char arrayBuf[kMaxLogBatch * 2048 + 64];
			int pos = 0;
			int remaining = static_cast<int>(sizeof(arrayBuf));
			pos += snprintf(arrayBuf + pos, remaining - pos,
				"{\"topic\":\"observation.log_batch\",\"entries\":[");
			for (int i = 0; i < mLogBatchCount && pos < remaining - 4; ++i)
			{
				if (i > 0) arrayBuf[pos++] = ',';
				int written = snprintf(arrayBuf + pos, remaining - pos, "%s", mLogBatch[i]);
				if (written < 0 || written >= remaining - pos) break;
				pos += written;
			}
			snprintf(arrayBuf + pos, remaining - pos, "]}");

			SendToSubscribers("observation.log", arrayBuf);
			mLogBatchCount = 0;
			mLogBatchElapsedSec = 0.0f;
		}

		void ObservationBridge::FlushTraceBatchLocked()
		{
			if (mTraceBatchCount == 0 || !mServer || !mSubscriberQuery)
				return;

			char arrayBuf[kMaxTraceBatch * 2048 + 64];
			int pos = 0;
			int remaining = static_cast<int>(sizeof(arrayBuf));
			pos += snprintf(arrayBuf + pos, remaining - pos,
				"{\"topic\":\"observation.trace_batch\",\"entries\":[");
			for (int i = 0; i < mTraceBatchCount && pos < remaining - 4; ++i)
			{
				if (i > 0) arrayBuf[pos++] = ',';
				int written = snprintf(arrayBuf + pos, remaining - pos, "%s", mTraceBatch[i]);
				if (written < 0 || written >= remaining - pos) break;
				pos += written;
			}
			snprintf(arrayBuf + pos, remaining - pos, "]}");

			SendToSubscribers("observation.trace", arrayBuf);
			mTraceBatchCount = 0;
			mTraceBatchElapsedSec = 0.0f;
		}

		void ObservationBridge::FlushMetricBatchLocked()
		{
			if (!mHasPendingMetric || !mServer || !mSubscriberQuery)
				return;

			SendToSubscribers("observation.metric", mLatestMetricBuf);
			mHasPendingMetric = false;
			mMetricElapsedSec = 0.0f;
		}

		//---------------------------------------------------------------------
		// Tick-driven flush
		//---------------------------------------------------------------------

		void ObservationBridge::Flush(float deltaTimeSec)
		{
			if (!mActive.load(std::memory_order_acquire))
				return;

			mLogBatchElapsedSec   += deltaTimeSec;
			mTraceBatchElapsedSec += deltaTimeSec;
			mMetricElapsedSec     += deltaTimeSec;

			static constexpr float kLogFlushIntervalSec    = 0.200f; // 200ms
			static constexpr float kTraceFlushIntervalSec  = 0.200f; // 200ms
			static constexpr float kMetricFlushIntervalSec = 0.500f; // 500ms

			if (mLogBatchCount > 0 && mLogBatchElapsedSec >= kLogFlushIntervalSec)
				FlushLogBatch();

			if (mTraceBatchCount > 0 && mTraceBatchElapsedSec >= kTraceFlushIntervalSec)
				FlushTraceBatch();

			if (mHasPendingMetric && mMetricElapsedSec >= kMetricFlushIntervalSec)
				FlushMetricBatch();
		}
	}
}
