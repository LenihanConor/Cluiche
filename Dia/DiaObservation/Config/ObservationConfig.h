#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Dia
{
	namespace Observation
	{
		enum class LogLevelConfig : uint8_t
		{
			kTrace,
			kDebug,
			kInfo,
			kWarning,
			kError,
			kDefault = kInfo
		};

		struct ObservationConfig
		{
			LogLevelConfig globalLogLevel = LogLevelConfig::kDefault;

			struct ChannelOverride
			{
				Dia::Core::StringCRC channel;
				LogLevelConfig       level;
			};
			ChannelOverride channelOverrides[16] = {};
			unsigned int    channelOverrideCount = 0;

			bool enableStdOutSink          = true;
			bool enableDebugOutputSink     = true;
			bool enableObservationFileSink = true;
			bool enableTraceFileSink       = true;
			bool enableMetricsFileSink     = true;

			bool     profileEnabled      = false;   // SD-O27: profiling defaults OFF
			uint32_t profileCategoryMask = 0;       // 0 = all categories disabled

			bool     traceEnabled        = false;   // SD-O27: tracing defaults OFF
			uint32_t traceCategoryMask   = 0;       // 0 = all categories disabled

			bool     enableHealthFileSink        = true;
			uint32_t metricSnapshotIntervalMs    = 100;
			uint32_t healthPollIntervalMs        = 500;

			// Perf Inspector A4a: trip detection config (applied to all PUs via Application)
			struct PerfConfig
			{
				uint32_t tripFrames    = 10;       // consecutive over-budget frames to trip
				float    tripFactor    = 2.0f;     // single frame > factor * budget trips
				uint32_t rearmFrames   = 30;       // consecutive under-budget frames to re-arm
				uint32_t minIntervalMs = 5000;     // min gap (ms) between emitted trips
				float    budgetScale   = 1.0f;     // multiplies budget; <= 0 means 1.0
				uint32_t perfExitSeconds = 0;      // exit after N seconds of profiling; 0 = never
			};
			PerfConfig perfConfig;
		};

	} // namespace Observation
} // namespace Dia
