////////////////////////////////////////////////////////////////////////////////
// Filename: Lifecycle.h
// Description: Python interpreter lifecycle management (initialize, shutdown, state)
// Feature spec: docs/specs/features/dia/diapython/interpreter-lifecycle.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Python
	{
		// Initialize Python interpreter
		// Parameters:
		//   pythonHome - Path to embedded Python runtime (e.g., "External/Python311/")
		//   modulePath - Path to custom Python modules (e.g., "External/Python/")
		//   captureWarnings - If true, Python warnings are captured and logged (default: false)
		// Returns: true on success, false on failure (logs error)
		bool Initialize(const char* pythonHome, const char* modulePath, bool captureWarnings = false);

		// Shutdown Python interpreter
		void Shutdown();

		// Check if Python is initialized
		bool IsInitialized();

		// Events (to be implemented with DiaCore/Architecture/Observer):
		// - OnPythonInitialized() - After Python interpreter starts successfully
		// - OnPythonShutdown() - Before Python interpreter shuts down
	}
}
