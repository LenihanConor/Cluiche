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

			// Additional internal structures will be added in later phases:
			// - PythonObjectImpl (type-conversion)
			// - PythonArgsImpl (type-conversion)
			// - LastError (error-handling)
			// - ModuleImpl, FunctionRegistration (module-api)
			// - AsyncTask, OutputRedirection (script-execution)
		}
	}
}
