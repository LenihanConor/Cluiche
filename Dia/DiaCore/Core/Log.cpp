// ===================================================================
// Log.cpp
// Simple logging system using Windows OutputDebugString
// ===================================================================

#include "DiaCore/Core/Log.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <windows.h>

#include "DiaCore/Strings/String1024.h"

namespace Dia
{
	namespace Core
	{
		// Output raw string to debug console
		// Text appears in Visual Studio Output window when debugging
		void Log::Output(const char* str)
		{
			OutputDebugStringA(str);
		}

		// Output string followed by newline
		void Log::OutputLine(const char* str)
		{
			OutputDebugStringA(str);
			OutputDebugStringA("\n");
		}

		// Output formatted string with printf-style arguments
		// Supports format specifiers like %d, %s, %f, etc.
		void Log::OutputVaradicLine(const char* str, ...)
		{
			va_list va;
			va_start(va, str);

			Dia::Core::Containers::String1024 newStr;

			if (str)
			{
				newStr.Format(str, va);
			}

			va_end(va);

			Dia::Core::Log::OutputLine(newStr.AsCStr());
		}
	}
}