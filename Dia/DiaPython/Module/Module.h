////////////////////////////////////////////////////////////////////////////////
// Filename: Module.h
// Description: Python module creation and function registration API
// Feature spec: docs/specs/features/dia/diapython/module-api.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "TypeConversion/PythonObject.h"
#include <functional>

namespace Dia
{
	namespace Python
	{
		// Opaque handle to Python module (hides pybind11::module_)
		class Module
		{
		public:
			// No public constructor - use CreateModule()
			// Module cannot be copied or assigned
			Module(const Module&) = delete;
			Module& operator=(const Module&) = delete;

		private:
			Module() = default;  // Private constructor
			friend Module* CreateModule(const char* name);
		};

		// Python function callback signature
		// Parameters:
		//   args - Function arguments from Python
		// Returns: Return value sent back to Python (or PythonObject() for None)
		// Note: C++ exceptions are caught and converted to Python exceptions
		using PythonCallback = std::function<PythonObject(const PythonArgs& args)>;

		// Create a new Python module
		// Parameters:
		//   name - Module name (e.g., "dia_cli" or "dia.cli" for nested)
		// Returns: Module handle, or nullptr if name invalid or module already exists
		// Note: If Python not initialized, module is cached and registered on Initialize()
		Module* CreateModule(const char* name);

		// Get an existing Python module
		// Parameters:
		//   name - Module name
		// Returns: Module handle, or nullptr if module doesn't exist
		Module* GetModule(const char* name);

		// Register a C++ function in a Python module
		// Parameters:
		//   module - Module created by CreateModule()
		//   functionName - Python function name
		//   callback - C++ function to invoke
		//   docstring - Optional documentation string (shown in Python help())
		// Note: Duplicate names log warning; last registration wins
		void AddFunction(
			Module* module,
			const char* functionName,
			PythonCallback callback,
			const char* docstring = nullptr
		);

		// Register overloaded function (same name, different signature hint)
		// Parameters:
		//   signatureHint - Human-readable signature for Python help (e.g., "(int, str)")
		void AddFunctionOverload(
			Module* module,
			const char* functionName,
			PythonCallback callback,
			const char* signatureHint,
			const char* docstring = nullptr
		);
	}
}
