////////////////////////////////////////////////////////////////////////////////
// Filename: DiaPythonInternal.h
// Description: Internal header for DiaPython - wraps pybind11 (NOT for public API)
// WARNING: This header includes pybind11 and should NEVER be included in public headers
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include "ErrorHandling/Error.h"  // For ErrorCode enum

namespace py = pybind11;

namespace Dia
{
	namespace Python
	{
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
				py::args* pyArgs = nullptr;  // Pointer to pybind11::args (not owned)
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

			// Additional internal structures will be added in later phases:
			// - ModuleImpl, FunctionRegistration (module-api)
			// - AsyncTask, OutputRedirection (script-execution)
		}
	}
}
