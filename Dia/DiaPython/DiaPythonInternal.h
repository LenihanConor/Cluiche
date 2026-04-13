////////////////////////////////////////////////////////////////////////////////
// Filename: DiaPythonInternal.h
// Description: Internal header for DiaPython - wraps pybind11 (NOT for public API)
// WARNING: This header includes pybind11 and should NEVER be included in public headers
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>

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

			// Additional internal structures will be added in later phases:
			// - LastError (error-handling)
			// - ModuleImpl, FunctionRegistration (module-api)
			// - AsyncTask, OutputRedirection (script-execution)
		}
	}
}
