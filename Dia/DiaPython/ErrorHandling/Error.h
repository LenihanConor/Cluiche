////////////////////////////////////////////////////////////////////////////////
// Filename: Error.h
// Description: Python error handling and exception conversion
// Feature spec: docs/specs/features/dia/diapython/error-handling.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Python
	{
		// Error codes returned by DiaPython functions
		enum class ErrorCode
		{
			Success = 0,
			GeneralError = 1,
			FileNotFound = 2,
			NotInitialized = 3,
			SyntaxError = 4,
			RuntimeException = 5,
			TypeError = 6,
			InitializationFailed = 7
		};

		// Event: Fired when any Python error occurs
		// Parameters:
		//   errorType - Python exception type name (e.g., "ValueError", "SyntaxError")
		//   errorMessage - Full error message with traceback
		//   context - Where the error occurred (e.g., "ExecuteScript", "ToInt")
		// TODO: Phase 7 - Implement as Observer pattern event
		// For now, this is documented as a future event
		// void OnPythonError(const char* errorType, const char* errorMessage, const char* context);
	}
}
