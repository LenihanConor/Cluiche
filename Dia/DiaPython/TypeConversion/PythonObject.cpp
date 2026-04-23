////////////////////////////////////////////////////////////////////////////////
// Filename: PythonObject.cpp
// Description: PythonObject and PythonArgs implementation
////////////////////////////////////////////////////////////////////////////////
#include "PythonObject.h"
#include "DiaPython/DiaPythonInternal.h"

#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Python
	{
		//======================================================================
		// PythonObject Implementation
		//======================================================================

		PythonObject::PythonObject()
		{
			// Create None object using py::none()
			auto* impl = new Internal::PythonObjectImpl();
			impl->pyObject = new py::object(py::none());
			mImpl = impl;
		}

		PythonObject::~PythonObject()
		{
			if (mImpl)
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				if (impl->pyObject)
				{
					delete impl->pyObject;
				}
				delete impl;
				mImpl = nullptr;
			}
		}

		PythonObject::PythonObject(const PythonObject& other)
		{
			if (other.mImpl)
			{
				auto* otherImpl = static_cast<Internal::PythonObjectImpl*>(other.mImpl);
				auto* impl = new Internal::PythonObjectImpl();

				// Copy py::object (reference counted by pybind11)
				if (otherImpl->pyObject)
				{
					impl->pyObject = new py::object(*otherImpl->pyObject);
				}
				else
				{
					impl->pyObject = nullptr;
				}

				impl->stringCache = otherImpl->stringCache;
				mImpl = impl;
			}
			else
			{
				// Other is uninitialized, create None
				mImpl = nullptr;
				auto* impl = new Internal::PythonObjectImpl();
				impl->pyObject = nullptr;
				mImpl = impl;
			}
		}

		PythonObject& PythonObject::operator=(const PythonObject& other)
		{
			if (this != &other)
			{
				// Clean up existing
				if (mImpl)
				{
					auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
					if (impl->pyObject)
					{
						delete impl->pyObject;
					}
					delete impl;
				}

				// Copy from other
				if (other.mImpl)
				{
					auto* otherImpl = static_cast<Internal::PythonObjectImpl*>(other.mImpl);
					auto* impl = new Internal::PythonObjectImpl();

					if (otherImpl->pyObject)
					{
						impl->pyObject = new py::object(*otherImpl->pyObject);
					}
					else
					{
						impl->pyObject = nullptr;
					}

					impl->stringCache = otherImpl->stringCache;
					mImpl = impl;
				}
				else
				{
					// Other is uninitialized
					auto* impl = new Internal::PythonObjectImpl();
					impl->pyObject = nullptr;
					mImpl = impl;
				}
			}
			return *this;
		}

		bool PythonObject::IsNone() const
		{
			if (!mImpl) return true;

			auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
			if (!impl->pyObject) return true;

			//Use pybind11's is_none() method to check if object is Python's None
			try
			{
				return impl->pyObject->is_none();
			}
			catch (...)
			{
				// If is_none() throws, treat as None
				return true;
			}
		}

		bool PythonObject::IsValid() const
		{
			if (!mImpl) return false;

			auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
			if (!impl->pyObject) return false;

			// Valid means not None
			try
			{
				return !impl->pyObject->is_none();
			}
			catch (...)
			{
				return false;
			}
		}

		bool PythonObject::IsInt() const
		{
			if (!mImpl || IsNone()) return false;

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				if (!impl->pyObject) return false;

				// Python bool is a subclass of int. We must explicitly exclude bools.
				// Use PyBool_Check and PyLong_Check from Python C API
				PyObject* ptr = impl->pyObject->ptr();
				if (!ptr) return false;

				// Check if it's a bool first (bools are ints in Python)
				if (PyBool_Check(ptr)) return false;

				// Check if it's an int
				return PyLong_Check(ptr);
			}
			catch (...)
			{
				return false;
			}
		}

		bool PythonObject::IsFloat() const
		{
			if (!mImpl || IsNone()) return false;

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				if (!impl->pyObject) return false;
				return py::isinstance<py::float_>(*impl->pyObject);
			}
			catch (...)
			{
				return false;
			}
		}

		bool PythonObject::IsBool() const
		{
			if (!mImpl || IsNone()) return false;

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				if (!impl->pyObject) return false;
				return py::isinstance<py::bool_>(*impl->pyObject);
			}
			catch (...)
			{
				return false;
			}
		}

		bool PythonObject::IsString() const
		{
			if (!mImpl || IsNone()) return false;

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				if (!impl->pyObject) return false;
				return py::isinstance<py::str>(*impl->pyObject);
			}
			catch (...)
			{
				return false;
			}
		}

		//======================================================================
		// PythonArgs Implementation
		//======================================================================

		// Constructor - initialize mImpl to nullptr in header
		// Note: PythonArgs is typically created via SetImpl() from Module.cpp

		int PythonArgs::GetCount() const
		{
			if (!mImpl) return 0;

			try
			{
				auto* impl = static_cast<Internal::PythonArgsImpl*>(mImpl);
				return static_cast<int>(impl->args.size());
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "PythonArgs::GetCount() failed: %s", ex.what());
				return 0;
			}
		}

		PythonObject PythonArgs::GetArg(int index) const
		{
			PythonObject result;  // Default is None

			if (!mImpl)
			{
				DIA_LOG_WARNING("DiaPython", "PythonArgs::GetArg(%d) called on null args", index);
				return result;
			}

			try
			{
				auto* impl = static_cast<Internal::PythonArgsImpl*>(mImpl);

				if (index < 0 || index >= static_cast<int>(impl->args.size()))
				{
					DIA_LOG_WARNING("DiaPython", "PythonArgs::GetArg(%d) index out of bounds (count=%d)",
						index, static_cast<int>(impl->args.size()));
					return result;
				}

				// Get the argument from vector (args is vector<py::object*>)
				py::object* argObjPtr = impl->args[index];
				if (!argObjPtr)
				{
					DIA_LOG_WARNING("DiaPython", "PythonArgs::GetArg(%d) argument pointer is null", index);
					return result;
				}

				auto* resultImpl = static_cast<Internal::PythonObjectImpl*>(result.mImpl);
				if (resultImpl->pyObject)
				{
					delete resultImpl->pyObject;
				}
				resultImpl->pyObject = new py::object(*argObjPtr);

				return result;
			}
			catch (const std::exception& ex)
			{
				DIA_LOG_ERROR("DiaPython", "PythonArgs::GetArg(%d) failed: %s", index, ex.what());
				return result;
			}
		}
	}
}
