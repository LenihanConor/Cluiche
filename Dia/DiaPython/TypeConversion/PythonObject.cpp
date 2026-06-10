////////////////////////////////////////////////////////////////////////////////
// Filename: PythonObject.cpp
// Description: PythonObject and PythonArgs implementation
////////////////////////////////////////////////////////////////////////////////
#include "PythonObject.h"
#include "DiaPython/DiaPythonInternal.h"
#include "DiaPython/Lifecycle/Lifecycle.h"

#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
	namespace Python
	{
		//======================================================================
		// PythonObject Implementation
		//======================================================================

		PythonObject::PythonObject()
		{
			auto* impl = new Internal::PythonObjectImpl();
			if (IsInitialized())
			{
				py::gil_scoped_acquire acquire;
				impl->pyObject = new py::object(py::none());
			}
			else
			{
				impl->pyObject = nullptr;
			}
			mImpl = impl;
		}

		PythonObject::~PythonObject()
		{
			if (mImpl)
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				if (impl->pyObject)
				{
					if (IsInitialized())
					{
						py::gil_scoped_acquire acquire;
						delete impl->pyObject;
					}
					else
					{
						// Python shut down — leak the handle to avoid crash.
						// This is the pybind11-recommended approach for statics/globals
						// that outlive the interpreter.
						impl->pyObject = nullptr;
					}
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

				if (otherImpl->pyObject && IsInitialized())
				{
					py::gil_scoped_acquire acquire;
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
				auto* impl = new Internal::PythonObjectImpl();
				impl->pyObject = nullptr;
				mImpl = impl;
			}
		}

		PythonObject& PythonObject::operator=(const PythonObject& other)
		{
			if (this != &other)
			{
				// Clean up existing (needs GIL for dec_ref)
				if (mImpl)
				{
					auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
					if (impl->pyObject && IsInitialized())
					{
						py::gil_scoped_acquire acquire;
						delete impl->pyObject;
					}
					delete impl;
				}

				// Copy from other
				if (other.mImpl)
				{
					auto* otherImpl = static_cast<Internal::PythonObjectImpl*>(other.mImpl);
					auto* impl = new Internal::PythonObjectImpl();

					if (otherImpl->pyObject && IsInitialized())
					{
						py::gil_scoped_acquire acquire;
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
			if (!IsInitialized()) return true;

			py::gil_scoped_acquire acquire;
			try
			{
				return impl->pyObject->is_none();
			}
			catch (...)
			{
				return true;
			}
		}

		bool PythonObject::IsValid() const
		{
			if (!mImpl) return false;

			auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
			if (!impl->pyObject) return false;
			if (!IsInitialized()) return false;

			py::gil_scoped_acquire acquire;
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
			if (!IsInitialized()) return false;

			py::gil_scoped_acquire acquire;
			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				if (!impl->pyObject) return false;

				PyObject* ptr = impl->pyObject->ptr();
				if (!ptr) return false;

				if (PyBool_Check(ptr)) return false;
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
			if (!IsInitialized()) return false;

			py::gil_scoped_acquire acquire;
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
			if (!IsInitialized()) return false;

			py::gil_scoped_acquire acquire;
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
			if (!IsInitialized()) return false;

			py::gil_scoped_acquire acquire;
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
