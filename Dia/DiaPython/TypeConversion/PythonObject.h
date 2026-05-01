////////////////////////////////////////////////////////////////////////////////
// Filename: PythonObject.h
// Description: PythonObject and PythonArgs opaque wrappers (hides pybind11)
// Feature spec: docs/specs/features/dia/diapython/type-conversion.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Python
	{
		// Forward declarations for conversion functions
		class PythonArgs;

		// Opaque wrapper for Python object (hides pybind11::object)
		class PythonObject
		{
		public:
			// Constructors
			PythonObject();  // Creates None/null object
			~PythonObject();

			// Copyable (reference counted, like Python objects)
			PythonObject(const PythonObject& other);
			PythonObject& operator=(const PythonObject& other);

			// State queries
			bool IsNone() const;      // True if object is None/null
			bool IsValid() const;     // True if object is valid (not None)

			// Type queries
			bool IsInt() const;
			bool IsFloat() const;
			bool IsBool() const;
			bool IsString() const;

			// Internal access methods (for Module.cpp and WrapCallback)
			// Note: These are public but should only be used internally
			void* GetImpl() const { return mImpl; }
			void SetImpl(void* impl) { mImpl = impl; }

		private:
			void* mImpl;  // PythonObjectImpl* (opaque)

			// Friend declarations for internal access
			friend class PythonArgs;
			friend PythonObject ToPython(int);
			friend PythonObject ToPython(float);
			friend PythonObject ToPython(bool);
			friend PythonObject ToPython(const char*);
			friend int ToInt(const PythonObject&);
			friend float ToFloat(const PythonObject&);
			friend bool ToBool(const PythonObject&);
			friend const char* ToString(const PythonObject&);
		};

		// Opaque wrapper for Python function arguments (hides pybind11::args)
		class PythonArgs
		{
		public:
			// Constructor
			PythonArgs() : mImpl(nullptr) {}

			// Get number of arguments
			int GetCount() const;

			// Get argument by index (0-based)
			// Returns: PythonObject, or None if index out of bounds
			PythonObject GetArg(int index) const;

			// Internal access methods (for Module.cpp and WrapCallback)
			// Note: These are public but should only be used internally
			void SetImpl(void* impl) { mImpl = impl; }

		private:
			void* mImpl;  // PythonArgsImpl* (opaque)

			// Friend declarations for internal access
			friend class Module;  // Module creates PythonArgs from pybind11::args
		};
	}
}
