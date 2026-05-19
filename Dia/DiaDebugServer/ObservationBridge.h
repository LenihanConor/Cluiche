#pragma once

#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Trace/ITraceSink.h>
#include <DiaObservation/Metric/IMetricSink.h>
#include <DiaObservation/Health/IHealthTransitionSink.h>

#include <atomic>
#include <cstdint>

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
			explicit ObservationBridge(Dia::WebSocket::Server* server);
			~ObservationBridge() override;

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

		private:
			Dia::WebSocket::Server* mServer;
			char mSessionId[32];
			int64_t mEpochOffsetNs;
			std::atomic<bool> mActive;
		};
	}
}
