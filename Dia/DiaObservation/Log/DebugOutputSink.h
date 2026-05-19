#pragma once

#include <DiaObservation/Log/ISink.h>

namespace Dia
{
	namespace Observation { namespace Log
	{
		class DebugOutputSink : public ISink
		{
		public:
			void OnLogEntry(const LogEntry& entry) override;
			const char* GetName() const override { return "DebugOutput"; }
		};
	}
} // namespace Observation
} // namespace Dia
