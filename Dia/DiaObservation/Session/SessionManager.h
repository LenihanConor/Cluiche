#pragma once

#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Config/ObservationConfig.h>
#include <DiaObservation/Metric/MetricSnapshot.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>
#include <mutex>
#include <atomic>

namespace Dia { namespace Observation { namespace Capture { class IFrameCaptureSource; } } }
namespace Dia { namespace Observation { namespace Log { class ObservationFileSink; } } }
namespace Dia { namespace Observation { namespace Trace { class Tracer; } } }
namespace Dia { namespace Observation { namespace Metric { class MetricsFileSink; } } }
namespace Dia { namespace Observation { namespace Health { class HealthRegistry; } } }
namespace Dia { namespace Observation { namespace Profile { class Profiler; } } }
namespace Dia { namespace Observation { namespace Capture { class CaptureManager; } } }

namespace Dia
{
	namespace Observation
	{
		struct SessionConfig
		{
			char appName[64];
			char buildVersion[64];
			char buildConfig[16];
			char outRootDir[256];
		};

		class SessionManager
		{
		public:
			SessionManager();
			~SessionManager();

			bool Start(const SessionConfig& config, const ObservationConfig& obsConfig = ObservationConfig());
			void Tick(float deltaTime);
			void Stop();

			bool IsStarted() const { return mStarted; }
			const char* GetSessionId() const { return mSessionId; }
			const char* GetSessionDirectory() const { return mSessionDir; }

			void PushScenarioStep(const Dia::Core::StringCRC& step);
			void PopScenarioStep();
			Dia::Core::StringCRC GetCurrentScenarioStep() const;

			void IncrementFrameCount();
			uint64_t GetFrameCount() const { return mFrameCount; }

			void SetCaptureSource(Capture::IFrameCaptureSource* source);
			Capture::CaptureManager* GetCaptureManager();

			static Capture::CaptureManager* GetActiveCaptureManager();
			static void SetActiveCaptureSource(Capture::IFrameCaptureSource* source);

			void OnRetainableEntry(const Log::LogEntry& entry);

			void EmergencyDump();

		private:
			void WriteSessionJson(const char* exitReason, int exitCode);
			void WriteHealthJson();
			void WriteCrashJson();
			void PollAndEmitHealthTransitions();
			bool CreateSessionDirectory();

			bool mStarted;
			char mSessionId[32];
			char mSessionDir[512];
			SessionConfig mConfig;

			Log::ObservationFileSink*       mObservationFileSink;
			Metric::MetricsFileSink*        mMetricsFileSink;
			Capture::CaptureManager*        mCaptureManager;

			uint32_t mMetricSnapshotIntervalMs;
			float    mMetricSnapshotAccumMs;
			int64_t  mEpochOffsetNs;
			Metric::MetricSnapshot mLastSnapshot;

			float    mHealthPollIntervalMs;
			float    mHealthPollAccumMs;
			bool     mEnableHealthFileSink;

			uint64_t mStartTimeUnixNano;
			uint64_t mFrameCount;
			uint32_t mErrorCount;
			uint32_t mWarningCount;

			static constexpr unsigned int kRetentionCapacity = 256;
			Log::LogEntry* mRetentionRing;
			unsigned int mRetentionWriteIndex;
			unsigned int mRetentionCount;
			std::mutex mRetentionMutex;

			std::atomic_flag mEmergencyDumpGuard;

			static SessionManager* sActiveInstance;
			static void TerminateHandler();
			static void RetentionCallbackStatic(const Log::LogEntry& entry, void* userData);
		};
	}
} // namespace Dia
