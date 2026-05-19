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
		};

	} // namespace Observation
} // namespace Dia
