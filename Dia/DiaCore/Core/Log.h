#ifndef DIA_LOG_H
#define DIA_LOG_H


namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Log
		//
		// Simple logging system that outputs to the debugger console.
		//
		// Uses Windows OutputDebugString for debug output (viewable in Visual Studio Output window,
		// DebugView, or other debug output capture tools).
		//
		// USAGE:
		//   Log::Output("Hello");
		//   Log::OutputLine("World");
		//   Log::OutputVaradicLine("Value: %d, Name: %s", 42, "test");
		//
		// FEATURES:
		//   - Zero overhead in release builds (assertions that use Log compile out)
		//   - Printf-style formatting via OutputVaradicLine
		//   - Thread-safe (OutputDebugString is thread-safe)
		//
		// NOTE: For file logging or more advanced logging, consider extending this class
		//---------------------------------------------------------------------------------------------------------------------------------
		class Log
		{
		public:
			// Output raw string to debug console (no newline)
			static void Output(const char* str);

			// Output string with automatic newline
			static void OutputLine(const char* str);

			// Output formatted string with printf-style arguments (with newline)
			static void OutputVaradicLine(const char* str, ...);
		};
	}
}

#endif // DIA_ASSERT