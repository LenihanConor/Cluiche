////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion.h
// Description: Type conversion functions (ToPython, ToInt, ToFloat, etc.)
// Feature spec: docs/specs/features/dia/diapython/type-conversion.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "PythonObject.h"

namespace Dia
{
	namespace Python
	{
		//======================================================================
		// C++ → Python Conversion
		//======================================================================

		// Convert int to Python int
		PythonObject ToPython(int value);

		// Convert float to Python float
		PythonObject ToPython(float value);

		// Convert bool to Python bool
		PythonObject ToPython(bool value);

		// Convert C string to Python string
		// Note: nullptr converts to None
		PythonObject ToPython(const char* str);

		//======================================================================
		// Python → C++ Conversion
		//======================================================================

		// Convert Python object to int
		// Returns: int value, or 0 on failure (logs error)
		// Note: Follows Python coercion (int("123") = 123, int(42.7) = 42)
		int ToInt(const PythonObject& obj);

		// Convert Python object to float
		// Returns: float value, or 0.0f on failure (logs error)
		// Note: Follows Python coercion (float(42) = 42.0, float("3.14") = 3.14)
		float ToFloat(const PythonObject& obj);

		// Convert Python object to bool
		// Returns: bool value, or false on failure (logs error)
		// Note: Follows Python truthiness (bool(0) = false, bool("") = false, etc.)
		bool ToBool(const PythonObject& obj);

		// Convert Python object to C string
		// Returns: const char* (internal buffer), or "" on failure (logs error)
		// Note: String is valid until PythonObject is destroyed or modified
		// Warning: Do not free returned pointer
		const char* ToString(const PythonObject& obj);
	}
}
