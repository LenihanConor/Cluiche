////////////////////////////////////////////////////////////////////////////////
// Filename: PythonObject.cpp
// Description: PythonObject and PythonArgs implementation
////////////////////////////////////////////////////////////////////////////////
#include "PythonObject.h"
#include "../DiaPythonInternal.h"

#include <DiaCore/Core/Logging/Logger.h>

namespace Dia
{
	namespace Python
	{
		//======================================================================
		// PythonObject Implementation
		//======================================================================

		PythonObject::PythonObject()
		{
			// Create None object
			auto* impl = new Internal::PythonObjectImpl();
			impl->pyObject = new py::object();  // Default constructor creates None
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
					impl->pyObject = new py::object();
				}

				impl->stringCache = otherImpl->stringCache;
				mImpl = impl;
			}
			else
			{
				// Other is uninitialized, create None
				mImpl = nullptr;
				auto* impl = new Internal::PythonObjectImpl();
				impl->pyObject = new py::object();
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
						impl->pyObject = new py::object();
					}

					impl->stringCache = otherImpl->stringCache;
					mImpl = impl;
				}
				else
				{
					// Other is uninitialized
					auto* impl = new Internal::PythonObjectImpl();
					impl->pyObject = new py::object();
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

			return impl->pyObject->is_none();
		}

		bool PythonObject::IsValid() const
		{
			return !IsNone();
		}

		bool PythonObject::IsInt() const
		{
			if (!mImpl || IsNone()) return false;

			try
			{
				auto* impl = static_cast<Internal::PythonObjectImpl*>(mImpl);
				return py::isinstance<py::int_>(*impl->pyObject);
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

		int PythonArgs::GetCount() const
		{
			if (!mImpl) return 0;

			try
			{
				auto* impl = static_cast<Internal::PythonArgsImpl*>(mImpl);
				if (!impl->pyArgs) return 0;

				return static_cast<int>(impl->pyArgs->size());
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
				if (!impl->pyArgs)
				{
					DIA_LOG_WARNING("DiaPython", "PythonArgs::GetArg(%d) called on null args", index);
					return result;
				}

				if (index < 0 || index >= static_cast<int>(impl->pyArgs->size()))
				{
					DIA_LOG_WARNING("DiaPython", "PythonArgs::GetArg(%d) index out of bounds (count=%d)",
						index, static_cast<int>(impl->pyArgs->size()));
					return result;
				}

				// Get the argument and wrap in PythonObject
				py::object argObj = (*impl->pyArgs)[index];

				auto* resultImpl = static_cast<Internal::PythonObjectImpl*>(result.mImpl);
				if (resultImpl->pyObject)
				{
					delete resultImpl->pyObject;
				}
				resultImpl->pyObject = new py::object(argObj);

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
