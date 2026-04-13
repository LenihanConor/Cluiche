////////////////////////////////////////////////////////////////////////////////
// Filename: Lifecycle.cpp
// Description: Python interpreter lifecycle management implementation
////////////////////////////////////////////////////////////////////////////////
#include "Lifecycle.h"
#include "../DiaPythonInternal.h"
#include "../TypeConversion/PythonObject.h"
#include "../Module/Module.h"

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

				// Mark as initialized BEFORE registering modules
				// (modules may call IsInitialized() during registration)
				gState.isInitialized = true;

				// Register pending modules (Phase 5: module-api)
				// Iterate through all modules and register those that are pending
				for (auto it = gModules.Begin(); it != gModules.End(); ++it)
				{
					ModuleImpl* moduleImpl = it.Value();
					if (moduleImpl && moduleImpl->isPendingRegistration)
					{
						try
						{
							// Create pybind11 module
							py::module_ newModule = py::module_::create_extension_module(
								moduleImpl->name.c_str(), nullptr, new py::module_::module_def());

							// Add to sys.modules
							sys.attr("modules")[moduleImpl->name.c_str()] = newModule;

							moduleImpl->pybindModule = new py::module_(newModule);
							moduleImpl->isPendingRegistration = false;

							// Register all pending functions
							for (unsigned int i = 0; i < moduleImpl->pendingFunctions.Size(); i++)
							{
								const FunctionRegistration& func = moduleImpl->pendingFunctions[i];

								// Wrap callback to catch C++ exceptions
								py::object wrappedCallback = py::cpp_function([func](py::args args) -> py::object
								{
									try
									{
										// Convert py::args to PythonArgs
										PythonArgs pythonArgs;
										PythonArgsImpl* argsImpl = new PythonArgsImpl();
										argsImpl->pyArgs = new py::args(args);
										pythonArgs.SetImpl(argsImpl);

										// Call user callback
										PythonObject result = func.callback(pythonArgs);

										// Convert PythonObject to py::object
										if (result.IsNone())
										{
											return py::none();
										}

										PythonObjectImpl* resultImpl = static_cast<PythonObjectImpl*>(result.GetImpl());
										if (resultImpl && resultImpl->pyObject)
										{
											return *resultImpl->pyObject;
										}

										return py::none();
									}
									catch (const std::exception& ex)
									{
										std::string errorMsg = std::string("C++ exception in ") + func.name + ": " + ex.what();
										throw py::type_error(errorMsg.c_str());
									}
									catch (...)
									{
										std::string errorMsg = std::string("Unknown C++ exception in ") + func.name;
										throw py::type_error(errorMsg.c_str());
									}
								});

								// Register function
								moduleImpl->pybindModule->attr(func.name.c_str()) = wrappedCallback;

								// Set docstring if provided
								if (!func.docstring.empty())
								{
									std::string fullDocstring = func.docstring;
									if (!func.signatureHint.empty())
									{
										fullDocstring = func.signatureHint + " - " + fullDocstring;
									}
									wrappedCallback.attr("__doc__") = py::str(fullDocstring.c_str());
								}

								DIA_LOG_INFO("DiaPython", "Registered pending function '%s' in module '%s'",
									func.name.c_str(), moduleImpl->name.c_str());
							}

							// Clear pending functions
							moduleImpl->pendingFunctions.RemoveAll();

							DIA_LOG_INFO("DiaPython", "Registered pending module '%s' with %d functions",
								moduleImpl->name.c_str(), moduleImpl->pendingFunctions.Size());
						}
						catch (const std::exception& ex)
						{
							DIA_LOG_ERROR("DiaPython", "Failed to register pending module '%s': %s",
								moduleImpl->name.c_str(), ex.what());
						}
					}
				}

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
