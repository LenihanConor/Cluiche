////////////////////////////////////////////////////////////////////////////////
// Filename: PythonBindings.h
// Description: Python bindings for DiaCLI commands
// Feature spec: docs/specs/features/dia/diacli/python-bindings.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace CLI
	{
		////////////////////////////////////////////////////////////////////////////////
		// Initialize Python bindings for all registered commands
		// Creates a "dia_cli" Python module with all registered commands as functions
		//
		// Prerequisites:
		//   - DiaPython::Initialize() must be called first
		//   - Commands should be registered before calling this
		//
		// Returns: void (logs warning if Python not initialized)
		////////////////////////////////////////////////////////////////////////////////
		void InitializePythonBindings();
	}
}
