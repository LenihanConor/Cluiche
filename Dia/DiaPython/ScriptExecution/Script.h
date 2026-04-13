////////////////////////////////////////////////////////////////////////////////
// Filename: Script.h
// Description: Python script execution API (sync/async)
// Feature spec: docs/specs/features/dia/diapython/script-execution.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <functional>

namespace Dia
{
	namespace Python
	{
		// Callback signature for async script execution completion
		// Parameters:
		//   exitCode - Script exit code (0 = success, non-zero = error)
		//   duration - Execution time in seconds
		using ScriptCompletionCallback = std::function<void(int exitCode, float duration)>;

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

		// Execute Python script file (asynchronous)
		// Parameters:
		//   scriptPath - Path to .py file
		//   args - Optional arguments
		//   argCount - Number of arguments
		//   callback - Called when script completes (on main thread)
		// Returns: immediately with task ID (0 = error starting script)
		// Pre-condition: Python is initialized
		int ExecuteScriptAsync(
			const char* scriptPath,
			const char** args,
			int argCount,
			ScriptCompletionCallback callback
		);

		// Execute Python code string (asynchronous)
		// Parameters:
		//   pythonCode - Python code to execute
		//   callback - Called when execution completes
		// Returns: immediately with task ID (0 = error starting execution)
		int ExecuteStringAsync(
			const char* pythonCode,
			ScriptCompletionCallback callback
		);

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

		// Cancel a specific async task
		// Parameters:
		//   taskId - Task ID returned by ExecuteScriptAsync/ExecuteStringAsync
		// Returns: true if task was cancelled, false if task not found or already completed
		bool CancelTask(int taskId);

		// Cancel all running async tasks
		// Returns: number of tasks cancelled
		int CancelAllTasks();

		// Events (TODO: Phase 7 - implement with Observer pattern)
		// void OnScriptExecuting(const char* scriptPath);
		// void OnScriptExecuted(const char* scriptPath, int exitCode, float duration);
		// void OnPythonError(const char* errorType, const char* errorMessage);
	}
}
