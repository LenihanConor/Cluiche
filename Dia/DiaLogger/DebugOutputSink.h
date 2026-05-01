#pragma once

#include <DiaLogger/ISink.h>

namespace Dia
{
	namespace Logger
	{
		class DebugOutputSink : public ISink
		{
		public:
			void OnLogEntry(const LogEntry& entry) override;
			const char* GetName() const override { return "DebugOutput"; }
		};
	}
}
