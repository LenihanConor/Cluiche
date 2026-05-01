////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion.cpp
// Description: Type conversion implementation
////////////////////////////////////////////////////////////////////////////////
#include "Conversion.h"
#include "DiaPython/DiaPythonInternal.h"
#include "DiaPython/Lifecycle/Lifecycle.h"

#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Python
	{
		//======================================================================
		// C++ → Python Conversion
		//======================================================================

		PythonObject ToPython(int value)
		{
			PythonObject result;

			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(int) called but Python is not initialized");
				return result;  // Return None
			}

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(result.mImpl);
				if (impl->pyObject)
				{
					delete impl->pyObject;
				}
				impl->pyObject = new py::object(py::cast(value));
				return result;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(int=%d) failed: %s", value, ex.what());
				return result;
			}
		}

		PythonObject ToPython(float value)
		{
			PythonObject result;

			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(float) called but Python is not initialized");
				return result;
			}

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(result.mImpl);
				if (impl->pyObject)
				{
					delete impl->pyObject;
				}
				impl->pyObject = new py::object(py::cast(value));
				return result;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(float=%f) failed: %s", value, ex.what());
				return result;
			}
		}

		PythonObject ToPython(bool value)
		{
			PythonObject result;

			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(bool) called but Python is not initialized");
				return result;
			}

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(result.mImpl);
				if (impl->pyObject)
				{
					delete impl->pyObject;
				}
				impl->pyObject = new py::object(py::cast(value));
				return result;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(bool=%d) failed: %s", value ? 1 : 0, ex.what());
				return result;
			}
		}

		PythonObject ToPython(const char* str)
		{
			PythonObject result;

			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(const char*) called but Python is not initialized");
				return result;
			}

			// nullptr converts to None (result already created as None)
			if (!str)
			{
				return result;
			}

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(result.mImpl);
				if (impl->pyObject)
				{
					delete impl->pyObject;
				}
				impl->pyObject = new py::object(py::cast(str));
				return result;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToPython(const char*) failed: %s", ex.what());
				return result;
			}
		}

		//======================================================================
		// Python → C++ Conversion
		//======================================================================

		int ToInt(const PythonObject& obj)
		{
			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToInt() called but Python is not initialized");
				return 0;
			}

			if (obj.IsNone())
			{
				DIA_LOG_WARNING("DiaPython", "ToInt() called on None, returning 0");
				return 0;
			}

			auto* impl = static_cast<Internal::PythonObjectImpl*>(obj.mImpl);
			if (!impl || !impl->pyObject)
			{
				DIA_LOG_ERROR("DiaPython", "ToInt() called on invalid PythonObject");
				return 0;
			}

			try
			{
				// Python coercion: int("123") = 123, int(42.7) = 42
				// Use Python's int() constructor for proper coercion
				py::int_ pyInt(*impl->pyObject);
				return py::cast<int>(pyInt);
			}
			catch (const py::cast_error& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToInt() failed: %s", ex.what());
				return 0;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToInt() failed: %s", ex.what());
				return 0;
			}
		}

		float ToFloat(const PythonObject& obj)
		{
			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToFloat() called but Python is not initialized");
				return 0.0f;
			}

			if (obj.IsNone())
			{
				DIA_LOG_WARNING("DiaPython", "ToFloat() called on None, returning 0.0f");
				return 0.0f;
			}

			auto* impl = static_cast<Internal::PythonObjectImpl*>(obj.mImpl);
			if (!impl || !impl->pyObject)
			{
				DIA_LOG_ERROR("DiaPython", "ToFloat() called on invalid PythonObject");
				return 0.0f;
			}

			try
			{
				// Python coercion: float(42) = 42.0, float("3.14") = 3.14
				// Use Python's float() constructor for proper coercion
				py::float_ pyFloat(*impl->pyObject);
				return py::cast<float>(pyFloat);
			}
			catch (const py::cast_error& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToFloat() failed: %s", ex.what());
				return 0.0f;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToFloat() failed: %s", ex.what());
				return 0.0f;
			}
		}

		bool ToBool(const PythonObject& obj)
		{
			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToBool() called but Python is not initialized");
				return false;
			}

			if (obj.IsNone())
			{
				DIA_LOG_WARNING("DiaPython", "ToBool() called on None, returning false");
				return false;
			}

			auto* impl = static_cast<Internal::PythonObjectImpl*>(obj.mImpl);
			if (!impl || !impl->pyObject)
			{
				DIA_LOG_ERROR("DiaPython", "ToBool() called on invalid PythonObject");
				return false;
			}

			try
			{
				// Python truthiness: bool(0) = false, bool("") = false, etc.
				return py::cast<bool>(*impl->pyObject);
			}
			catch (const py::cast_error& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToBool() failed: %s", ex.what());
				return false;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToBool() failed: %s", ex.what());
				return false;
			}
		}

		const char* ToString(const PythonObject& obj)
		{
			if (!IsInitialized())
			{
				DIA_LOG_ERROR("DiaPython", "ToString() called but Python is not initialized");
				return "";
			}

			if (obj.IsNone())
			{
				DIA_LOG_WARNING("DiaPython", "ToString() called on None, returning empty string");
				return "";
			}

			auto* impl = static_cast<Internal::PythonObjectImpl*>(obj.mImpl);
			if (!impl || !impl->pyObject)
			{
				DIA_LOG_ERROR("DiaPython", "ToString() called on invalid PythonObject");
				return "";
			}

			try
			{
				// Convert to Python string, then to C++ string
				std::string result = py::cast<std::string>(*impl->pyObject);

				// Cache the string in PythonObjectImpl
				impl->stringCache = result;

				// Return pointer to cached string
				return impl->stringCache.c_str();
			}
			catch (const py::cast_error& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToString() failed: %s", ex.what());
				impl->stringCache = "";
				return impl->stringCache.c_str();
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "ToString() failed: %s", ex.what());
				impl->stringCache = "";
				return impl->stringCache.c_str();
			}
		}
	}
}
