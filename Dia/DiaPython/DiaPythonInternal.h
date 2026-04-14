////////////////////////////////////////////////////////////////////////////////
// Filename: DiaPythonInternal.h
// Description: Internal header for DiaPython - wraps pybind11 (NOT for public API)
// WARNING: This header includes pybind11 and should NEVER be included in public headers
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include "ErrorHandling/Error.h"  // For ErrorCode enum
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Threading/Mutex.h>
#include <string>
#include <functional>
#include <unordered_map>

namespace py = pybind11;

namespace Dia
{
	namespace Python
	{
		// Forward declarations
		class PythonObject;
		class PythonArgs;

		namespace Internal
		{
			// Python interpreter state (singleton)
			struct InterpreterState
			{
				bool isInitialized = false;
				py::scoped_interpreter* interpreter = nullptr;
			};

			extern InterpreterState gState;

			// PythonObject implementation (Phase 3: type-conversion)
			struct PythonObjectImpl
			{
				py::object* pyObject = nullptr;  // Heap-allocated pybind11::object
				std::string stringCache;         // Cache for ToString() return value
			};

			// PythonArgs implementation (Phase 3: type-conversion)
			struct PythonArgsImpl
			{
				std::vector<py::object*> args;  // Heap-allocated py::objects
				~PythonArgsImpl()
				{
					for (auto* obj : args)
					{
						delete obj;
					}
				}
			};

			// Error handling structures (Phase 4: error-handling)
			struct ErrorContext
			{
				const char* operation = nullptr;   // What operation was being performed
				const char* scriptPath = nullptr;  // Script file path (if applicable)
				int lineNumber = 0;                // Line number in script (if applicable)
			};

			struct LastError
			{
				Dia::Python::ErrorCode code;  // Defaults to 0 (Success)
				std::string errorType;        // Python exception type name
				std::string message;          // Full error message with traceback
				std::string context;          // Where the error occurred
			};

			extern LastError gLastError;

			// Error handling functions (Phase 4: error-handling)
			Dia::Python::ErrorCode ConvertException(const py::error_already_set& ex, const ErrorContext& context);
			const char* ExtractTraceback(const py::error_already_set& ex);
			void ReportError(Dia::Python::ErrorCode code, const char* errorType, const char* message, const ErrorContext& context);
			const LastError& GetLastError();

			// Module API structures (Phase 5: module-api)
			using PythonCallback = std::function<Dia::Python::PythonObject(const Dia::Python::PythonArgs&)>;

			// Function registration (for deferred registration)
			struct FunctionRegistration
			{
				std::string name;
				PythonCallback callback;
				std::string docstring;
				std::string signatureHint;  // Empty for non-overloaded functions
			};

			// Module wrapper (hides pybind11::module_)
			struct ModuleImpl
			{
				std::string name;
				py::module_* pybindModule = nullptr;     // null if Python not initialized
				bool isPendingRegistration = false;
				Dia::Core::Containers::DynamicArrayC<FunctionRegistration, 32> pendingFunctions;
			};

			// Global module registry accessor (uses std::unordered_map to avoid DiaCore HashTable bugs)
			std::unordered_map<unsigned int, ModuleImpl*>& GetModuleRegistry();
			void ClearModuleRegistry();

			// Module API helper functions
			bool ValidateModuleName(const char* name);
			py::object WrapCallback(PythonCallback callback, const char* functionName);

			// Script Execution structures (script-execution feature - synchronous only)
			// Forward declaration from Script.h
			using OutputCallback = std::function<void(const char*)>;

			// Output redirection state
			struct OutputRedirection
			{
				OutputCallback stdoutCallback;
				OutputCallback stderrCallback;
				bool isRedirected;
				py::object originalStdout;  // Store original sys.stdout
				py::object originalStderr;  // Store original sys.stderr
			};

			// Global output redirection state
			extern OutputRedirection gOutputRedirection;

			// Script execution helper functions
			int ExecuteScriptInternal(const char* scriptPath, const char** args, int argCount);
			int ExecuteStringInternal(const char* pythonCode);
		}
	}
}
