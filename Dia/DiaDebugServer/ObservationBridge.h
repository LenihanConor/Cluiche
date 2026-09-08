#pragma once

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Trace/ITraceSink.h>
#include <DiaObservation/Metric/IMetricSink.h>
#include <DiaObservation/Health/IHealthTransitionSink.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>

namespace Dia { namespace WebSocket { class Server; } }

namespace Dia
{
	namespace DebugServer
	{
		class ObservationBridge
			: public Dia::Observation::Log::ISink
			, public Dia::Observation::Trace::ITraceSink
			, public Dia::Observation::Metric::IMetricSink
			, public Dia::Observation::Health::IHealthTransitionSink
		{
		public:
			// Callback to retrieve connection IDs subscribed to a given topic.
			// Called from observation sink threads — must be thread-safe (v1: accepted race).
			using SubscriberQueryFn = std::function<void(const char* topic, Dia::Core::Containers::DynamicArrayC<int, 16>& out)>;

			explicit ObservationBridge(Dia::WebSocket::Server* server);
			~ObservationBridge() override;

			void SetSubscriberQuery(SubscriberQueryFn fn);

			void Start(const char* sessionId, int64_t epochOffsetNs);
			void Stop();

			// ISink (Log)
			void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override;
			const char* GetName() const override { return "ObservationBridge"; }

			// ITraceSink
			void OnSpan(const Dia::Observation::Trace::SpanRecord& span) override;

			// IMetricSink
			void OnSnapshot(const Dia::Observation::Metric::MetricSnapshot& snapshot) override;
			void OnFinal(const Dia::Observation::Metric::MetricSnapshot& snapshot) override;

			// IHealthTransitionSink
			void OnTransition(const Dia::Observation::Health::HealthRegistry::Transition& transition) override;

			// Called each tick to flush pending batches. deltaTimeSec = frame time.
			void Flush(float deltaTimeSec);

		private:
			void SendToSubscribers(const char* topic, const char* buf);
			void FlushLogBatch();
			void FlushTraceBatch();
			void FlushMetricBatch();
			void FlushLogBatchLocked();
			void FlushTraceBatchLocked();
			void FlushMetricBatchLocked();

			Dia::WebSocket::Server* mServer;
			SubscriberQueryFn mSubscriberQuery;
			char mSessionId[32];
			int64_t mEpochOffsetNs;
			std::atomic<bool> mActive;

			// Log batch — accumulates up to kMaxLogBatch entries before flushing.
			static constexpr int kMaxLogBatch = 10;
			std::mutex mLogMutex;
			char  mLogBatch[kMaxLogBatch][2048];
			int   mLogBatchCount;
			float mLogBatchElapsedSec;

			// Trace batch — accumulates up to kMaxTraceBatch spans before flushing.
			static constexpr int kMaxTraceBatch = 5;
			std::mutex mTraceMutex;
			char  mTraceBatch[kMaxTraceBatch][2048];
			int   mTraceBatchCount;
			float mTraceBatchElapsedSec;

			// Metric latest-wins (single slot) — only the most recent snapshot is kept.
			std::mutex mMetricMutex;
			char  mLatestMetricBuf[4096];
			bool  mHasPendingMetric;
			float mMetricElapsedSec;
		};
	}
}
