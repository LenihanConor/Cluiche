////////////////////////////////////////////////////////////////////////////////
// Filename: Script.cpp
// Description: Python script execution implementation (sync/async)
// Feature spec: docs/specs/features/dia/diapython/script-execution.md
////////////////////////////////////////////////////////////////////////////////
#include "Script.h"
#include "../DiaPythonInternal.h"
#include "../Lifecycle/Lifecycle.h"
#include <DiaCore/Core/Logging/Logger.h>
#include <thread>
#include <chrono>
#include <cstdio>

namespace Dia
{
	namespace Python
	{
		namespace Internal
		{
			// Global script execution state
			Dia::Core::Containers::DynamicArrayC<AsyncTask, 16> gAsyncTasks;
			OutputRedirection gOutputRedirection = { nullptr, nullptr, false, py::object(), py::object() };
			int gNextTaskId = 1;
			Dia::Core::Mutex gAsyncTaskMutex;

			////////////////////////////////////////////////////////////////////////////////
			// Execute Python script from file (internal helper)
			////////////////////////////////////////////////////////////////////////////////
			int ExecuteScriptInternal(const char* scriptPath, const char** args, int argCount)
			{
				using namespace Dia::Core;

				// Validate Python is initialized
				if (!IsInitialized())
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteScript failed: Python not initialized");
					return static_cast<int>(ErrorCode::NotInitialized);
				}

				// Validate file exists
				if (!scriptPath || scriptPath[0] == '\0')
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteScript failed: scriptPath is null or empty");
					return static_cast<int>(ErrorCode::FileNotFound);
				}

				// Check if file exists using simple file open
				FILE* testFile = nullptr;
				#ifdef _MSC_VER
				fopen_s(&testFile, scriptPath, "r");
				#else
				testFile = fopen(scriptPath, "r");
				#endif

				if (!testFile)
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteScript failed: File not found: %s", scriptPath);
					return static_cast<int>(ErrorCode::FileNotFound);
				}
				fclose(testFile);

				// TODO: Phase 7 - Fire OnScriptExecuting event

				auto startTime = std::chrono::high_resolution_clock::now();

				try
				{
					// Set sys.argv
					py::module_ sys = py::module_::import("sys");
					py::list argv;
					argv.append(scriptPath);  // First arg is script path

					// Add user arguments
					for (int i = 0; i < argCount; i++)
					{
						if (args[i])
						{
							argv.append(args[i]);
						}
					}

					sys.attr("argv") = argv;

					// Execute script
					py::eval_file(scriptPath, py::globals());

					// Calculate duration
					auto endTime = std::chrono::high_resolution_clock::now();
					float duration = std::chrono::duration<float>(endTime - startTime).count();

					// TODO: Phase 7 - Fire OnScriptExecuted event

					DIA_LOG_INFO("DiaPython", "ExecuteScript succeeded: %s (%.3f seconds)", scriptPath, duration);
					return static_cast<int>(ErrorCode::Success);
				}
				catch (const py::error_already_set& ex)
				{
					// Calculate duration
					auto endTime = std::chrono::high_resolution_clock::now();
					float duration = std::chrono::duration<float>(endTime - startTime).count();

					// Convert Python exception to error code
					ErrorContext context;
					context.operation = "ExecuteScript";
					context.scriptPath = scriptPath;
					context.lineNumber = 0;

					ErrorCode errorCode = ConvertException(ex, context);

					// TODO: Phase 7 - Fire OnScriptExecuted event with error code
					// TODO: Phase 7 - Fire OnPythonError event

					return static_cast<int>(errorCode);
				}
				catch (const std::exception& ex)
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteScript failed with C++ exception: %s", ex.what());
					return static_cast<int>(ErrorCode::GeneralError);
				}
			}

			////////////////////////////////////////////////////////////////////////////////
			// Execute Python code string (internal helper)
			////////////////////////////////////////////////////////////////////////////////
			int ExecuteStringInternal(const char* pythonCode)
			{
				using namespace Dia::Core;

				// Validate Python is initialized
				if (!IsInitialized())
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteString failed: Python not initialized");
					return static_cast<int>(ErrorCode::NotInitialized);
				}

				// Validate code
				if (!pythonCode || pythonCode[0] == '\0')
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteString failed: pythonCode is null or empty");
					return static_cast<int>(ErrorCode::GeneralError);
				}

				// TODO: Phase 7 - Fire OnScriptExecuting event (scriptPath = "<string>")

				auto startTime = std::chrono::high_resolution_clock::now();

				try
				{
					// Execute code
					py::exec(pythonCode, py::globals());

					// Calculate duration
					auto endTime = std::chrono::high_resolution_clock::now();
					float duration = std::chrono::duration<float>(endTime - startTime).count();

					// TODO: Phase 7 - Fire OnScriptExecuted event

					DIA_LOG_INFO("DiaPython", "ExecuteString succeeded (%.3f seconds)", duration);
					return static_cast<int>(ErrorCode::Success);
				}
				catch (const py::error_already_set& ex)
				{
					// Calculate duration
					auto endTime = std::chrono::high_resolution_clock::now();
					float duration = std::chrono::duration<float>(endTime - startTime).count();

					// Convert Python exception to error code
					ErrorContext context;
					context.operation = "ExecuteString";
					context.scriptPath = "<string>";
					context.lineNumber = 0;

					ErrorCode errorCode = ConvertException(ex, context);

					// TODO: Phase 7 - Fire OnScriptExecuted event with error code
					// TODO: Phase 7 - Fire OnPythonError event

					return static_cast<int>(errorCode);
				}
				catch (const std::exception& ex)
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteString failed with C++ exception: %s", ex.what());
					return static_cast<int>(ErrorCode::GeneralError);
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Execute Python script file (synchronous)
		////////////////////////////////////////////////////////////////////////////////
		int ExecuteScript(const char* scriptPath, const char** args, int argCount)
		{
			return Internal::ExecuteScriptInternal(scriptPath, args, argCount);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Execute Python code string (synchronous)
		////////////////////////////////////////////////////////////////////////////////
		int ExecuteString(const char* pythonCode)
		{
			return Internal::ExecuteStringInternal(pythonCode);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Execute Python script file (asynchronous)
		////////////////////////////////////////////////////////////////////////////////
		int ExecuteScriptAsync(
			const char* scriptPath,
			const char** args,
			int argCount,
			ScriptCompletionCallback callback)
		{
			using namespace Internal;
			using namespace Dia::Core;

			// Validate inputs
			if (!scriptPath || scriptPath[0] == '\0')
			{
				DIA_LOG_ERROR("DiaPython", "ExecuteScriptAsync failed: scriptPath is null or empty");
				return 0;
			}

			if (!callback)
			{
				DIA_LOG_ERROR("DiaPython", "ExecuteScriptAsync failed: callback is null");
				return 0;
			}

			// Check async task limit (16 concurrent)
			{
				ScopedLock<Mutex> lock(gAsyncTaskMutex);
				if (gAsyncTasks.Size() >= 16)
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteScriptAsync failed: Maximum 16 concurrent tasks reached");
					return 0;
				}
			}

			// Create async task
			AsyncTask task;
			task.taskId = gNextTaskId++;
			task.scriptPath = scriptPath;
			task.code = "";  // Empty for file execution
			task.callback = callback;
			task.isRunning = true;
			task.isCancelled = false;

			// Copy args for thread
			std::vector<std::string> argsCopy;
			for (int i = 0; i < argCount; i++)
			{
				if (args[i])
				{
					argsCopy.push_back(args[i]);
				}
			}

			int taskId = task.taskId;

			// Add to task list
			{
				ScopedLock<Mutex> lock(gAsyncTaskMutex);
				gAsyncTasks.Add(task);
			}

			// Launch thread
			std::thread([taskId, scriptPath = std::string(scriptPath), argsCopy, callback]()
			{
				// Execute script (acquires GIL automatically via pybind11)
				// Create C-style args array
				std::vector<const char*> argsPtr;
				for (const auto& arg : argsCopy)
				{
					argsPtr.push_back(arg.c_str());
				}

				int exitCode = Internal::ExecuteScriptInternal(
					scriptPath.c_str(),
					argsPtr.empty() ? nullptr : argsPtr.data(),
					static_cast<int>(argsPtr.size())
				);

				// Mark task as complete
				bool wasCancelled = false;
				{
					ScopedLock<Mutex> lock(gAsyncTaskMutex);
					for (unsigned int i = 0; i < gAsyncTasks.Size(); i++)
					{
						if (gAsyncTasks[i].taskId == taskId)
						{
							wasCancelled = gAsyncTasks[i].isCancelled;
							gAsyncTasks.RemoveAt(i);
							break;
						}
					}
				}

				// Invoke callback on main thread
				// TODO: This needs proper main thread posting mechanism
				// For now, invoke directly (Phase 7 will integrate with event system)
				if (!wasCancelled && callback)
				{
					// Calculate duration (approximation)
					callback(exitCode, 0.0f);
				}

			}).detach();  // Detach thread

			DIA_LOG_INFO("DiaPython", "ExecuteScriptAsync started: %s (taskId=%d)", scriptPath, taskId);
			return taskId;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Execute Python code string (asynchronous)
		////////////////////////////////////////////////////////////////////////////////
		int ExecuteStringAsync(
			const char* pythonCode,
			ScriptCompletionCallback callback)
		{
			using namespace Internal;
			using namespace Dia::Core;

			// Validate inputs
			if (!pythonCode || pythonCode[0] == '\0')
			{
				DIA_LOG_ERROR("DiaPython", "ExecuteStringAsync failed: pythonCode is null or empty");
				return 0;
			}

			if (!callback)
			{
				DIA_LOG_ERROR("DiaPython", "ExecuteStringAsync failed: callback is null");
				return 0;
			}

			// Check async task limit (16 concurrent)
			{
				ScopedLock<Mutex> lock(gAsyncTaskMutex);
				if (gAsyncTasks.Size() >= 16)
				{
					DIA_LOG_ERROR("DiaPython", "ExecuteStringAsync failed: Maximum 16 concurrent tasks reached");
					return 0;
				}
			}

			// Create async task
			AsyncTask task;
			task.taskId = gNextTaskId++;
			task.scriptPath = "<string>";
			task.code = pythonCode;
			task.callback = callback;
			task.isRunning = true;
			task.isCancelled = false;

			int taskId = task.taskId;

			// Add to task list
			{
				ScopedLock<Mutex> lock(gAsyncTaskMutex);
				gAsyncTasks.Add(task);
			}

			// Launch thread
			std::thread([taskId, code = std::string(pythonCode), callback]()
			{
				// Execute code (acquires GIL automatically via pybind11)
				int exitCode = Internal::ExecuteStringInternal(code.c_str());

				// Mark task as complete
				bool wasCancelled = false;
				{
					ScopedLock<Mutex> lock(gAsyncTaskMutex);
					for (unsigned int i = 0; i < gAsyncTasks.Size(); i++)
					{
						if (gAsyncTasks[i].taskId == taskId)
						{
							wasCancelled = gAsyncTasks[i].isCancelled;
							gAsyncTasks.RemoveAt(i);
							break;
						}
					}
				}

				// Invoke callback on main thread
				// TODO: This needs proper main thread posting mechanism
				// For now, invoke directly (Phase 7 will integrate with event system)
				if (!wasCancelled && callback)
				{
					callback(exitCode, 0.0f);
				}

			}).detach();  // Detach thread

			DIA_LOG_INFO("DiaPython", "ExecuteStringAsync started (taskId=%d)", taskId);
			return taskId;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Redirect Python stdout/stderr to custom callbacks
		////////////////////////////////////////////////////////////////////////////////
		void RedirectOutput(
			OutputCallback stdoutCallback,
			OutputCallback stderrCallback)
		{
			using namespace Internal;

			// Validate Python is initialized
			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "RedirectOutput failed: Python not initialized");
				return;
			}

			try
			{
				py::module_ sys = py::module_::import("sys");

				// Store original stdout/stderr if not already redirected
				if (!gOutputRedirection.isRedirected)
				{
					gOutputRedirection.originalStdout = sys.attr("stdout");
					gOutputRedirection.originalStderr = sys.attr("stderr");
				}

				// Create custom writer class for stdout
				if (stdoutCallback)
				{
					py::object writer = py::cpp_function([stdoutCallback](const std::string& text)
					{
						if (stdoutCallback)
						{
							stdoutCallback(text.c_str());
						}
					});

					// Create simple writer object with write method
					py::module_ io = py::module_::import("io");
					py::object stringIO = io.attr("StringIO")();

					// Override stdout
					sys.attr("stdout") = py::cpp_function([stdoutCallback](const std::string& text)
					{
						if (stdoutCallback)
						{
							stdoutCallback(text.c_str());
						}
					});

					gOutputRedirection.stdoutCallback = stdoutCallback;
				}

				// Create custom writer class for stderr
				if (stderrCallback)
				{
					sys.attr("stderr") = py::cpp_function([stderrCallback](const std::string& text)
					{
						if (stderrCallback)
						{
							stderrCallback(text.c_str());
						}
					});

					gOutputRedirection.stderrCallback = stderrCallback;
				}

				gOutputRedirection.isRedirected = true;

				DIA_LOG_INFO("DiaPython", "Output redirection enabled");
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "RedirectOutput failed: %s", ex.what());
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Restore Python stdout/stderr to default behavior
		////////////////////////////////////////////////////////////////////////////////
		void RestoreOutput()
		{
			using namespace Internal;

			// Validate Python is initialized
			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "RestoreOutput failed: Python not initialized");
				return;
			}

			if (!gOutputRedirection.isRedirected)
			{
				DIA_LOG_WARNING("DiaPython", "RestoreOutput called but output not redirected");
				return;
			}

			try
			{
				py::module_ sys = py::module_::import("sys");

				// Restore original stdout/stderr
				if (!gOutputRedirection.originalStdout.is_none())
				{
					sys.attr("stdout") = gOutputRedirection.originalStdout;
				}

				if (!gOutputRedirection.originalStderr.is_none())
				{
					sys.attr("stderr") = gOutputRedirection.originalStderr;
				}

				// Clear redirection state
				gOutputRedirection.stdoutCallback = nullptr;
				gOutputRedirection.stderrCallback = nullptr;
				gOutputRedirection.isRedirected = false;
				gOutputRedirection.originalStdout = py::object();
				gOutputRedirection.originalStderr = py::object();

				DIA_LOG_INFO("DiaPython", "Output redirection restored to default");
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "RestoreOutput failed: %s", ex.what());
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Cancel a specific async task
		////////////////////////////////////////////////////////////////////////////////
		bool CancelTask(int taskId)
		{
			using namespace Internal;
			using namespace Dia::Core;

			ScopedLock<Mutex> lock(gAsyncTaskMutex);

			for (unsigned int i = 0; i < gAsyncTasks.Size(); i++)
			{
				if (gAsyncTasks[i].taskId == taskId)
				{
					if (gAsyncTasks[i].isRunning)
					{
						// Mark as cancelled (thread will check this flag)
						gAsyncTasks[i].isCancelled = true;

						DIA_LOG_INFO("DiaPython", "Cancelled async task %d", taskId);
						return true;
					}
				}
			}

			return false;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Cancel all running async tasks
		////////////////////////////////////////////////////////////////////////////////
		int CancelAllTasks()
		{
			using namespace Internal;
			using namespace Dia::Core;

			ScopedLock<Mutex> lock(gAsyncTaskMutex);

			int cancelledCount = 0;

			for (unsigned int i = 0; i < gAsyncTasks.Size(); i++)
			{
				if (gAsyncTasks[i].isRunning)
				{
					gAsyncTasks[i].isCancelled = true;
					cancelledCount++;
				}
			}

			if (cancelledCount > 0)
			{
				DIA_LOG_INFO("DiaPython", "Cancelled %d async tasks", cancelledCount);
			}

			return cancelledCount;
		}
	}
}
