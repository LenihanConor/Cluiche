////////////////////////////////////////////////////////////////////////////////
// Filename: Script.cpp
// Description: Python script execution implementation (synchronous only)
// Feature spec: docs/specs/features/dia/diapython/script-execution.md
////////////////////////////////////////////////////////////////////////////////
#include "Script.h"
#include "DiaPython/DiaPythonInternal.h"
#include "DiaPython/Lifecycle/Lifecycle.h"
#include <DiaLogger/DiaLog.h>
#include <chrono>
#include <cstdio>

namespace Dia
{
	namespace Python
	{
		namespace Internal
		{
			// Custom Python writer class for output redirection
			class CustomWriter
			{
			public:
				OutputCallback callback;

				CustomWriter(OutputCallback cb) : callback(cb) {}

				void write(const std::string& text)
				{
					if (callback)
					{
						callback(text.c_str());
					}
				}

				void flush() {}
			};

			// Global output redirection state
			OutputRedirection gOutputRedirection = { nullptr, nullptr, false, py::object(), py::object() };

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

				// Register CustomWriter class with pybind11
				// Note: We check if class exists because if Python restarts between tests,
				// the class needs to be re-registered
				try
				{
					py::object builtins = py::globals()["__builtins__"];
					if (!py::hasattr(builtins, "CustomWriter"))
					{
						py::class_<CustomWriter>(builtins, "CustomWriter")
							.def(py::init<OutputCallback>())
							.def("write", &CustomWriter::write)
							.def("flush", &CustomWriter::flush);
					}
				}
				catch (const std::exception& ex)
				{
					// If registration fails, log but try to continue
					DIA_LOG_WARNING("DiaPython", "CustomWriter class registration failed: %s", ex.what());
				}

				// Create custom writer for stdout
				if (stdoutCallback)
				{
					CustomWriter* writer = new CustomWriter(stdoutCallback);
					py::object writerObj = py::cast(writer);
					sys.attr("stdout") = writerObj;

					gOutputRedirection.stdoutCallback = stdoutCallback;
				}

				// Create custom writer for stderr
				if (stderrCallback)
				{
					CustomWriter* writer = new CustomWriter(stderrCallback);
					py::object writerObj = py::cast(writer);
					sys.attr("stderr") = writerObj;

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

	}
}
