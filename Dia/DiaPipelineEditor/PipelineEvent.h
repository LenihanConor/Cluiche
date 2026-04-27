#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace PipelineEditor
	{
		struct PipelineEvent
		{
			Dia::Core::StringCRC eventType;
			Dia::Core::StringCRC system;
			Dia::Core::StringCRC stage;
			Dia::Core::StringCRC step;
			float timestampSec = 0.0f;
			int durationMs = -1;
			const char* error = nullptr;
			const char* detail = nullptr;
			const char* level = nullptr;
		};

		struct RunSummary
		{
			Dia::Core::StringCRC target;
			Dia::Core::StringCRC config;
			int passCount = 0;
			int failCount = 0;
			int totalDurationMs = 0;
			float startTimestamp = 0.0f;
			bool interrupted = false;
		};
	}
}
