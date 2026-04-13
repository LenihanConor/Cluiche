////////////////////////////////////////////////////////////////////////////////
// Filename: Module.cpp
// Description: Python module API implementation
// Feature spec: docs/specs/features/dia/diapython/module-api.md
////////////////////////////////////////////////////////////////////////////////
#include "Module.h"
#include "../DiaPythonInternal.h"
#include "../Lifecycle/Lifecycle.h"
#include <DiaCore/Core/Logging/Logger.h>
#include <cctype>

namespace Dia
{
	namespace Python
	{
		namespace Internal
		{
			// Global module registry
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, ModuleImpl*> gModules;

			////////////////////////////////////////////////////////////////////////////////
			// Validate module name (lowercase_with_underscores or dot.separated)
			////////////////////////////////////////////////////////////////////////////////
			bool ValidateModuleName(const char* name)
			{
				if (!name || name[0] == '\0')
				{
					return false;  // Empty name
				}

				// Check each character
				for (const char* p = name; *p != '\0'; ++p)
				{
					char c = *p;
					bool valid = (c >= 'a' && c <= 'z') ||
					             (c >= '0' && c <= '9') ||
					             (c == '_') ||
					             (c == '.');  // Allow dots for nested modules

					if (!valid)
					{
						return false;
					}
				}

				return true;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Wrap C++ callback to catch exceptions and convert to Python
			////////////////////////////////////////////////////////////////////////////////
			py::object WrapCallback(PythonCallback callback, const char* functionName)
			{
				return py::cpp_function([callback, functionName](py::args args) -> py::object
				{
					try
					{
						// Convert py::args to PythonArgs
						PythonArgs pythonArgs;
						PythonArgsImpl* argsImpl = new PythonArgsImpl();
						argsImpl->pyArgs = new py::args(args);
						pythonArgs.SetImpl(argsImpl);

						// Call user callback
						PythonObject result = callback(pythonArgs);

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
						// Convert C++ exception to Python exception
						std::string errorMsg = std::string("C++ exception in ") + functionName + ": " + ex.what();
						throw py::type_error(errorMsg.c_str());
					}
					catch (...)
					{
						// Unknown exception
						std::string errorMsg = std::string("Unknown C++ exception in ") + functionName;
						throw py::type_error(errorMsg.c_str());
					}
				});
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Create a new Python module
		////////////////////////////////////////////////////////////////////////////////
		Module* CreateModule(const char* name)
		{
			using namespace Internal;

			// Validate module name
			if (!ValidateModuleName(name))
			{
				DIA_LOG_ERROR("DiaPython", "CreateModule failed: Invalid module name '%s'", name ? name : "(null)");
				return nullptr;
			}

			// Check if module already exists
			Dia::Core::StringCRC nameCRC(name);
			if (gModules.ContainsKey(nameCRC))
			{
				DIA_LOG_WARNING("DiaPython", "CreateModule: Module '%s' already exists", name);
				return nullptr;
			}

			// Check hard limit of 32 modules
			if (gModules.Size() >= 32)
			{
				DIA_LOG_ERROR("DiaPython", "CreateModule failed: Maximum 32 modules limit reached");
				return nullptr;
			}

			// Create module impl
			ModuleImpl* moduleImpl = new ModuleImpl();
			moduleImpl->name = name;

			// If Python initialized, create pybind11 module immediately
			if (IsInitialized())
			{
				try
				{
					// Support nested modules via dot notation (e.g., "dia.cli")
					moduleImpl->pybindModule = new py::module_(py::module_::import("sys").attr("modules"));

					// Create module using pybind11
					py::module_ mainModule = py::module_::import("__main__");
					py::module_ newModule = py::module_::create_extension_module(name, nullptr, new py::module_::module_def());

					// Add to sys.modules
					py::module_::import("sys").attr("modules")[name] = newModule;

					moduleImpl->pybindModule = new py::module_(newModule);
					moduleImpl->isPendingRegistration = false;
				}
				catch (const std::exception& ex)
				{
					DIA_LOG_ERROR("DiaPython", "CreateModule failed to create pybind11 module: %s", ex.what());
					delete moduleImpl;
					return nullptr;
				}
			}
			else
			{
				// Python not initialized - mark as pending
				moduleImpl->isPendingRegistration = true;
				DIA_LOG_INFO("DiaPython", "CreateModule: Module '%s' pending registration (Python not initialized)", name);
			}

			// Register in global module registry
			gModules.Add(nameCRC, moduleImpl);

			// Return opaque Module pointer (cast ModuleImpl* to Module*)
			return reinterpret_cast<Module*>(moduleImpl);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Get an existing Python module
		////////////////////////////////////////////////////////////////////////////////
		Module* GetModule(const char* name)
		{
			using namespace Internal;

			if (!name)
			{
				return nullptr;
			}

			Dia::Core::StringCRC nameCRC(name);
			ModuleImpl** moduleImplPtr = gModules.TryGetItem(nameCRC);

			if (moduleImplPtr && *moduleImplPtr)
			{
				return reinterpret_cast<Module*>(*moduleImplPtr);
			}

			return nullptr;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Register a C++ function in a Python module
		////////////////////////////////////////////////////////////////////////////////
		void AddFunction(
			Module* module,
			const char* functionName,
			PythonCallback callback,
			const char* docstring)
		{
			using namespace Internal;

			// Validate inputs
			if (!module)
			{
				DIA_LOG_ERROR("DiaPython", "AddFunction failed: module is null");
				return;
			}

			if (!functionName || functionName[0] == '\0')
			{
				DIA_LOG_ERROR("DiaPython", "AddFunction failed: functionName is empty");
				return;
			}

			if (!callback)
			{
				DIA_LOG_ERROR("DiaPython", "AddFunction failed: callback is null");
				return;
			}

			// Cast Module* back to ModuleImpl*
			ModuleImpl* moduleImpl = reinterpret_cast<ModuleImpl*>(module);

			// If module has pybind11 handle, register immediately
			if (moduleImpl->pybindModule)
			{
				try
				{
					// Wrap callback to catch C++ exceptions
					py::object wrappedCallback = WrapCallback(callback, functionName);

					// Register with pybind11
					moduleImpl->pybindModule->attr(functionName) = wrappedCallback;

					// Set docstring if provided
					if (docstring && docstring[0] != '\0')
					{
						wrappedCallback.attr("__doc__") = py::str(docstring);
					}

					DIA_LOG_INFO("DiaPython", "AddFunction: Registered '%s' in module '%s'", functionName, moduleImpl->name.c_str());
				}
				catch (const std::exception& ex)
				{
					DIA_LOG_ERROR("DiaPython", "AddFunction failed to register '%s': %s", functionName, ex.what());
				}
			}
			else
			{
				// Module pending registration - add to pending functions list
				FunctionRegistration registration;
				registration.name = functionName;
				registration.callback = callback;
				registration.docstring = docstring ? docstring : "";
				registration.signatureHint = "";  // Non-overloaded

				moduleImpl->pendingFunctions.Add(registration);

				DIA_LOG_INFO("DiaPython", "AddFunction: Queued '%s' for module '%s' (pending registration)", functionName, moduleImpl->name.c_str());
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Register overloaded function (same name, different signature hint)
		////////////////////////////////////////////////////////////////////////////////
		void AddFunctionOverload(
			Module* module,
			const char* functionName,
			PythonCallback callback,
			const char* signatureHint,
			const char* docstring)
		{
			using namespace Internal;

			// Validate inputs
			if (!module)
			{
				DIA_LOG_ERROR("DiaPython", "AddFunctionOverload failed: module is null");
				return;
			}

			if (!functionName || functionName[0] == '\0')
			{
				DIA_LOG_ERROR("DiaPython", "AddFunctionOverload failed: functionName is empty");
				return;
			}

			if (!callback)
			{
				DIA_LOG_ERROR("DiaPython", "AddFunctionOverload failed: callback is null");
				return;
			}

			// Cast Module* back to ModuleImpl*
			ModuleImpl* moduleImpl = reinterpret_cast<ModuleImpl*>(module);

			// If module has pybind11 handle, register immediately
			if (moduleImpl->pybindModule)
			{
				try
				{
					// Wrap callback to catch C++ exceptions
					py::object wrappedCallback = WrapCallback(callback, functionName);

					// Register with pybind11
					// For overloaded functions, we need to handle differently
					// For now, just add with signature hint in docstring
					std::string fullDocstring = signatureHint ? std::string(signatureHint) : "";
					if (docstring && docstring[0] != '\0')
					{
						if (!fullDocstring.empty()) fullDocstring += " - ";
						fullDocstring += docstring;
					}

					moduleImpl->pybindModule->attr(functionName) = wrappedCallback;
					if (!fullDocstring.empty())
					{
						wrappedCallback.attr("__doc__") = py::str(fullDocstring.c_str());
					}

					DIA_LOG_INFO("DiaPython", "AddFunctionOverload: Registered '%s%s' in module '%s'",
						functionName, signatureHint ? signatureHint : "", moduleImpl->name.c_str());
				}
				catch (const std::exception& ex)
				{
					DIA_LOG_ERROR("DiaPython", "AddFunctionOverload failed to register '%s': %s", functionName, ex.what());
				}
			}
			else
			{
				// Module pending registration - add to pending functions list
				FunctionRegistration registration;
				registration.name = functionName;
				registration.callback = callback;
				registration.docstring = docstring ? docstring : "";
				registration.signatureHint = signatureHint ? signatureHint : "";

				moduleImpl->pendingFunctions.Add(registration);

				DIA_LOG_INFO("DiaPython", "AddFunctionOverload: Queued '%s%s' for module '%s' (pending registration)",
					functionName, signatureHint ? signatureHint : "", moduleImpl->name.c_str());
			}
		}
	}
}
