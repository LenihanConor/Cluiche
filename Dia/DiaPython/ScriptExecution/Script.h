////////////////////////////////////////////////////////////////////////////////
// Filename: Script.h
// Description: Python script execution API (synchronous only)
// Feature spec: docs/specs/features/dia/diapython/script-execution.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <functional>

namespace Dia
{
	namespace Python
	{
		// Callback signature for output redirection
		// Parameters:
		//   text - Output text from Python stdout/stderr
		using OutputCallback = std::function<void(const char* text)>;

		// Execute Python script file (synchronous)
		// Parameters:
		//   scriptPath - Path to .py file
		//   args - Optional arguments passed to script (array of strings)
		//   argCount - Number of arguments in args array
		// Returns: exit code (0 = success, non-zero = error)
		// Pre-condition: Python is initialized
		int ExecuteScript(const char* scriptPath, const char** args = nullptr, int argCount = 0);

		// Execute Python code string (synchronous)
		// Parameters:
		//   pythonCode - Python code to execute
		// Returns: exit code (0 = success, non-zero = error)
		// Pre-condition: Python is initialized
		int ExecuteString(const char* pythonCode);

		// Redirect Python stdout/stderr to custom callbacks
		// Parameters:
		//   stdoutCallback - Called for each line of stdout (nullptr to leave unchanged)
		//   stderrCallback - Called for each line of stderr (nullptr to leave unchanged)
		// Note: Redirection is global and affects all script executions
		void RedirectOutput(
			OutputCallback stdoutCallback,
			OutputCallback stderrCallback
		);

		// Restore Python stdout/stderr to default behavior
		void RestoreOutput();

		// Events (TODO: implement with Observer pattern)
		// void OnScriptExecuting(const char* scriptPath);
		// void OnScriptExecuted(const char* scriptPath, int exitCode, float duration);
		// void OnPythonError(const char* errorType, const char* errorMessage);
	}
}
