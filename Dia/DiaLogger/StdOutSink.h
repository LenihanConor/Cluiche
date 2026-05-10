#pragma once

#include <DiaLogger/ISink.h>

namespace Dia
{
	namespace Logger
	{
		// Writes each log entry to stdout as a line. Useful for running under
		// bash/CI where OutputDebugString (Visual Studio Output window) is
		// not visible.
		class StdOutSink : public ISink
		{
		public:
			void OnLogEntry(const LogEntry& entry) override;
			const char* GetName() const override { return "StdOut"; }
		};
	}
}
