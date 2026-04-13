////////////////////////////////////////////////////////////////////////////////
// Filename: Lifecycle.cpp
// Description: Python interpreter lifecycle management implementation
////////////////////////////////////////////////////////////////////////////////
#include "Lifecycle.h"
#include "../DiaPythonInternal.h"

#include <DiaCore/Core/Logging/Logger.h>

namespace Dia
{
	namespace Python
	{
		namespace Internal
		{
			// Global interpreter state
			InterpreterState gState;
		}

		bool Initialize(const char* pythonHome, const char* modulePath, bool captureWarnings)
		{
			using namespace Internal;

			// Check if already initialized (idempotent)
			if (gState.isInitialized)
			{
				DIA_LOG_WARNING("DiaPython", "Initialize() called but Python is already initialized. Ignoring.");
				return true;  // Already initialized, return success
			}

			try
			{
				// Create Python interpreter (pybind11 RAII handle)
				gState.interpreter = new py::scoped_interpreter();

				// Configure sys.path
				py::module_ sys = py::module_::import("sys");
				py::list path = sys.attr("path");

				// Add pythonHome if provided
				if (pythonHome && pythonHome[0] != '\0')
				{
					path.append(pythonHome);
					DIA_LOG_INFO("DiaPython", "Added pythonHome to sys.path: %s", pythonHome);
				}

				// Add modulePath if provided
				if (modulePath && modulePath[0] != '\0')
				{
					path.append(modulePath);
					DIA_LOG_INFO("DiaPython", "Added modulePath to sys.path: %s", modulePath);
				}

				// Add current directory
				path.append(".");

				// Capture warnings if requested
				if (captureWarnings)
				{
					py::module_ warnings = py::module_::import("warnings");
					warnings.attr("simplefilter")("always");
					DIA_LOG_INFO("DiaPython", "Python warnings capture enabled");
				}

				// Mark as initialized
				gState.isInitialized = true;

				// TODO: Fire OnPythonInitialized event (requires Observer pattern integration)

				DIA_LOG_INFO("DiaPython", "Python interpreter initialized successfully");
				return true;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "Failed to initialize Python interpreter: %s", ex.what());

				// Clean up on failure
				if (gState.interpreter)
				{
					delete gState.interpreter;
					gState.interpreter = nullptr;
				}
				gState.isInitialized = false;

				return false;
			}
		}

		void Shutdown()
		{
			using namespace Internal;

			// Check if initialized
			if (!gState.isInitialized)
			{
				DIA_LOG_WARNING("DiaPython", "Shutdown() called but Python is not initialized. Ignoring.");
				return;
			}

			try
			{
				// TODO: Fire OnPythonShutdown event (requires Observer pattern integration)

				// Delete Python interpreter (RAII cleanup)
				if (gState.interpreter)
				{
					delete gState.interpreter;
					gState.interpreter = nullptr;
				}

				gState.isInitialized = false;

				DIA_LOG_INFO("DiaPython", "Python interpreter shut down successfully");
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "Error during Python shutdown: %s", ex.what());

				// Force cleanup even on error
				if (gState.interpreter)
				{
					delete gState.interpreter;
					gState.interpreter = nullptr;
				}
				gState.isInitialized = false;
			}
		}

		bool IsInitialized()
		{
			return Internal::gState.isInitialized;
		}
	}
}
