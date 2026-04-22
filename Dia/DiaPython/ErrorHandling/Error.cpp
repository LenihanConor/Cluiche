////////////////////////////////////////////////////////////////////////////////
// Filename: Error.cpp
// Description: Python error handling and exception conversion implementation
// Feature spec: docs/specs/features/dia/diapython/error-handling.md
////////////////////////////////////////////////////////////////////////////////
#include "Error.h"
#include "../DiaPythonInternal.h"

#include <DiaLogger/DiaLog.h>
#include <sstream>

namespace Dia
{
	namespace Python
	{
		namespace Internal
		{
			LastError gLastError;

			////////////////////////////////////////////////////////////////////////////////
			// Convert Python exception to ErrorCode
			////////////////////////////////////////////////////////////////////////////////
			ErrorCode ConvertException(const py::error_already_set& ex, const ErrorContext& context)
			{
				ErrorCode code = ErrorCode::GeneralError;
				std::string errorType = "UnknownError";

				if (ex.matches(PyExc_SyntaxError))
				{
					code = ErrorCode::SyntaxError;
					errorType = "SyntaxError";
				}
				else if (ex.matches(PyExc_RuntimeError))
				{
					code = ErrorCode::RuntimeException;
					errorType = "RuntimeError";
				}
				else if (ex.matches(PyExc_TypeError))
				{
					code = ErrorCode::TypeError;
					errorType = "TypeError";
				}
				else if (ex.matches(PyExc_FileNotFoundError))
				{
					code = ErrorCode::FileNotFound;
					errorType = "FileNotFoundError";
				}
				else if (ex.matches(PyExc_Exception))
				{
					code = ErrorCode::RuntimeException;
					errorType = "Exception";
				}

				// Extract traceback
				const char* traceback = ExtractTraceback(ex);

				// Report error
				ReportError(code, errorType.c_str(), traceback, context);

				return code;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Extract Python traceback as string (truncate at 50 frames)
			////////////////////////////////////////////////////////////////////////////////
			const char* ExtractTraceback(const py::error_already_set& ex)
			{
				static std::string tracebackCache;
				tracebackCache.clear();

				try
				{
					// Get exception info
					std::string exceptionStr = ex.what();
					tracebackCache = exceptionStr;

					// Try to get full traceback from Python
					if (gState.isInitialized && ex.type())
					{
						py::module_ traceback_module = py::module_::import("traceback");
						py::object format_exception = traceback_module.attr("format_exception");

						py::object formatted = format_exception(ex.type(), ex.value(), ex.trace());
						py::list lines = py::cast<py::list>(formatted);

						std::ostringstream oss;
						int frameCount = 0;
						const int maxFrames = 50;

						for (auto line : lines)
						{
							if (frameCount >= maxFrames)
							{
								oss << "\n... (traceback truncated at " << maxFrames << " frames)";
								break;
							}
							oss << py::cast<std::string>(line);
							frameCount++;
						}

						tracebackCache = oss.str();
					}
				}
				catch (...)
				{
					// If traceback extraction fails, use basic what() string
					tracebackCache = ex.what();
				}

				return tracebackCache.c_str();
			}

			////////////////////////////////////////////////////////////////////////////////
			// Report error - log, fire event, store in gLastError
			////////////////////////////////////////////////////////////////////////////////
			void ReportError(ErrorCode code, const char* errorType, const char* message, const ErrorContext& context)
			{
				// Store in gLastError
				gLastError.code = code;
				gLastError.errorType = errorType ? errorType : "";
				gLastError.message = message ? message : "";

				// Build context string
				std::ostringstream contextStream;
				if (context.operation)
				{
					contextStream << context.operation;
				}
				if (context.scriptPath)
				{
					if (context.operation) contextStream << " - ";
					contextStream << context.scriptPath;
				}
				if (context.lineNumber > 0)
				{
					contextStream << ":" << context.lineNumber;
				}
				gLastError.context = contextStream.str();

				// Log error
				DIA_LOG_ERROR("DiaPython", "[%s] %s (Context: %s)", errorType, message, gLastError.context.c_str());

				// TODO: Phase 7 - Fire OnPythonError event via Observer pattern
				// For now, just log. Event system integration deferred to Phase 7.
			}

			////////////////////////////////////////////////////////////////////////////////
			// Get last error (for debugging)
			////////////////////////////////////////////////////////////////////////////////
			const LastError& GetLastError()
			{
				return gLastError;
			}
		}
	}
}
