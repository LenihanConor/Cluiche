# Feature Spec: Type Conversion

## Traceability

| Level | Spec | Link |
|-------|------|------|
| **Feature** | type-conversion | (this document) |
| **System** | DiaPython | @docs/specs/systems/dia/diapython.md |
| **Application** | Dia | @docs/specs/applications/dia.md |
| **Platform** | Cluiche | @docs/specs/platform/Cluiche.md |

## Problem Statement

Provides bidirectional C++ ↔ Python type conversion utilities so C++ functions can exchange data with Python scripts without exposing pybind11 types.

## Acceptance Criteria

1. ✅ Can convert C++ int → PythonObject via `ToPython(int)`
2. ✅ Can convert C++ float → PythonObject via `ToPython(float)`
3. ✅ Can convert C++ bool → PythonObject via `ToPython(bool)`
4. ✅ Can convert C++ const char* → PythonObject via `ToPython(const char*)`
5. ✅ Can convert PythonObject → C++ int via `ToInt(PythonObject)`
6. ✅ Can convert PythonObject → C++ float via `ToFloat(PythonObject)`
7. ✅ Can convert PythonObject → C++ bool via `ToBool(PythonObject)`
8. ✅ Can convert PythonObject → C++ const char* via `ToString(PythonObject)`
9. ✅ PythonArgs provides GetCount() and GetArg(index)
10. ✅ Type conversion validates types (e.g., ToInt on string returns error)
11. ✅ Invalid conversions logged with helpful error messages
12. ✅ PythonObject can represent None/null values
13. ✅ No pybind11 types exposed in public API
14. ✅ Implicit type coercion follows Python rules (e.g., int("123") works)
15. ✅ PythonObject lifetime managed automatically (reference counted)

## API Design

### PythonObject Class

```cpp
namespace Dia::Python {
    // Opaque wrapper for Python object (hides pybind11::object)
    class PythonObject {
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
        
    private:
        void* mImpl;  // pybind11::object* (allocated on heap)
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
}
```

### PythonArgs Class

```cpp
namespace Dia::Python {
    // Opaque wrapper for Python function arguments (hides pybind11::args)
    class PythonArgs {
    public:
        // Get number of arguments
        int GetCount() const;
        
        // Get argument by index (0-based)
        // Returns: PythonObject, or None if index out of bounds
        PythonObject GetArg(int index) const;
        
    private:
        void* mImpl;  // pybind11::args*
        friend class Module;  // Module creates PythonArgs from pybind11::args
    };
}
```

### Conversion Functions

```cpp
namespace Dia::Python {
    // ===== C++ → Python =====
    
    // Convert int to Python int
    PythonObject ToPython(int value);
    
    // Convert float to Python float
    PythonObject ToPython(float value);
    
    // Convert bool to Python bool
    PythonObject ToPython(bool value);
    
    // Convert C string to Python string
    // Note: nullptr converts to None
    PythonObject ToPython(const char* str);
    
    // ===== Python → C++ =====
    
    // Convert Python object to int
    // Returns: int value, or 0 on failure (logs error)
    // Note: Follows Python coercion (int("123") = 123, int(42.7) = 42)
    int ToInt(const PythonObject& obj);
    
    // Convert Python object to float
    // Returns: float value, or 0.0f on failure (logs error)
    // Note: Follows Python coercion (float(42) = 42.0, float("3.14") = 3.14)
    float ToFloat(const PythonObject& obj);
    
    // Convert Python object to bool
    // Returns: bool value, or false on failure (logs error)
    // Note: Follows Python truthiness (bool(0) = false, bool("") = false, etc.)
    bool ToBool(const PythonObject& obj);
    
    // Convert Python object to C string
    // Returns: const char* (internal buffer), or "" on failure (logs error)
    // Note: String is valid until PythonObject is destroyed or modified
    // Warning: Do not free returned pointer
    const char* ToString(const PythonObject& obj);
}
```

## Data Models

### Internal Implementation

```cpp
namespace Dia::Python {
    namespace Internal {
        // PythonObject implementation
        // Wraps pybind11::object with heap allocation
        struct PythonObjectImpl {
            py::object* pyObject;  // Heap-allocated pybind11::object
            std::string stringCache;  // Cache for ToString() return value
        };
        
        // PythonArgs implementation
        struct PythonArgsImpl {
            py::args* pyArgs;  // Pointer to pybind11::args (not owned)
        };
    }
}
```

## Implementation Tasks

1. **Implement PythonObject class**
   - Constructor: allocate PythonObjectImpl, create py::object (None)
   - Destructor: delete PythonObjectImpl, release py::object
   - Copy constructor: deep copy (reference counted by pybind11)
   - Assignment: deep copy with self-assignment check
   - IsNone(): check if py::object is None
   - IsValid(): check if py::object is not None
   - Type queries (IsInt, IsFloat, etc.): use pybind11 type checks

2. **Implement PythonArgs class**
   - GetCount(): return py::args.size()
   - GetArg(index): bounds check, wrap py::args[index] in PythonObject

3. **Implement ToPython() conversions**
   - ToPython(int): create py::int_, wrap in PythonObject
   - ToPython(float): create py::float_, wrap in PythonObject
   - ToPython(bool): create py::bool_, wrap in PythonObject
   - ToPython(const char*): handle nullptr → None, else create py::str

4. **Implement ToInt() conversion**
   - Check if None (return 0, log warning)
   - Try py::cast<int> with Python coercion
   - Catch exceptions, log error, return 0

5. **Implement ToFloat() conversion**
   - Check if None (return 0.0f, log warning)
   - Try py::cast<float> with Python coercion
   - Catch exceptions, log error, return 0.0f

6. **Implement ToBool() conversion**
   - Check if None (return false, log warning)
   - Use Python truthiness rules (py::cast<bool>)
   - Catch exceptions, log error, return false

7. **Implement ToString() conversion**
   - Check if None (return "", log warning)
   - Try py::cast<std::string>
   - Cache string in PythonObjectImpl.stringCache
   - Return const char* to cached string
   - Catch exceptions, log error, return ""

8. **Add error logging**
   - Log type mismatches (e.g., "ToInt failed: Python object is string 'abc'")
   - Log None conversions (warning level)
   - Include Python type name in error messages

9. **Write unit tests**
   - Test: ToPython(int) creates Python int
   - Test: ToPython(float) creates Python float
   - Test: ToPython(bool) creates Python bool
   - Test: ToPython(const char*) creates Python string
   - Test: ToPython(nullptr) creates None
   - Test: ToInt with Python int
   - Test: ToInt with Python string "123" (coercion)
   - Test: ToInt with Python float 42.7 → 42 (coercion)
   - Test: ToInt with invalid type (logs error, returns 0)
   - Test: ToFloat with Python float
   - Test: ToFloat with Python int (coercion)
   - Test: ToFloat with Python string "3.14" (coercion)
   - Test: ToBool with various Python values (truthiness)
   - Test: ToString with Python string
   - Test: ToString with Python int (coercion)
   - Test: PythonObject copy constructor (reference counting)
   - Test: PythonArgs GetCount/GetArg
   - Test: PythonObject IsNone/IsValid/IsInt/IsFloat/etc.

## Files Affected

### New Files
- `Dia/DiaPython/DiaPythonObject.h` - PythonObject/PythonArgs class declarations
- `Dia/DiaPython/DiaPythonObject.cpp` - PythonObject/PythonArgs implementation
- `Dia/DiaPython/DiaPythonConversion.h` - ToPython/ToInt/ToFloat etc. declarations
- `Dia/DiaPython/DiaPythonConversion.cpp` - Conversion function implementations
- `Cluiche/Tests/UnitTests/DiaPythonConversionTests.cpp` - Unit tests

### Modified Files
- `Dia/DiaPython/DiaPython.h` - Include DiaPythonObject.h and DiaPythonConversion.h
- `Dia/DiaPython/DiaPythonInternal.h` - Add PythonObjectImpl, PythonArgsImpl

## Dependencies

### Dia Systems
- **interpreter-lifecycle** - Conversions require Python to be initialized
- **DiaCore/Core** - Logging for conversion errors
- **DiaCore/Strings** - String256 for error messages

### External
- **pybind11** - py::object, py::int_, py::float_, py::str, py::bool_, py::cast

## Open Questions

1. Should we support arrays/lists in phase 2?
2. Should we support DiaCore containers (DynamicArrayC → Python list)?
3. Should we add type conversion for double (in addition to float)?
4. Should ToString() return lifetime be extended beyond PythonObject lifetime?
5. Should we support custom type conversions (user-defined types)?

## Binding Decisions Compliance

| ID | Decision Summary | Compliance |
|----|------------------|------------|
| PD-001 | Use StringCRC for entity/component IDs | **Compliant** - No entity/component IDs in this feature. Type names are for error messages, not identifiers. |
| PD-004 | No STL containers in public APIs | **Compliant** - Public API uses only PythonObject, PythonArgs, primitives (int, float, bool, const char*). std::string used internally only. |
| PD-006 | Visual Studio project files are source of truth | **Compliant** - Implementation in existing DiaPython.vcxproj. |
| AD-001 | Module system with YAML frontmatter documentation | **Compliant** - Updates to existing dia.python.architecture.module.md. |
| AD-002 | No STL containers in public APIs | **Compliant** - Reinforces PD-004. Internal std::string usage is implementation detail. |
| AD-003 | Namespace convention: `Dia::<Module>::` | **Compliant** - All code in `Dia::Python::` namespace. |
| SD-001 | Wrap pybind11, don't expose it | **Compliant** - PythonObject/PythonArgs hide pybind11::object/args. All pybind11 types in .cpp files only. |
| SD-002 | Python interpreter is singleton | **Compliant** - Conversions use the single global Python interpreter. |
| SD-004 | Opaque handle types (Module, PythonObject) | **Compliant** - PythonObject/PythonArgs are opaque. pybind11 types hidden via void* mImpl. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Open Questions | Should we support arrays/lists in phase 2? | No - defer until concrete use case. Start with primitives only. |
| 2 | Open Questions | Should we support DiaCore containers? | No - defer until needed. Adds complexity and unclear semantics. |
| 3 | Open Questions | Should we add type conversion for double? | No - float only (simpler, covers most cases; can add double later if needed) |
| 4 | Open Questions | Should ToString() return lifetime be extended? | No - string valid only while PythonObject alive (simpler, caller responsibility to copy if needed) |
| 5 | Open Questions | Should we support custom type conversions? | No - primitives only (simpler, defer until concrete use case emerges) |
| 6 | Implementation | What happens if conversion called before Python initialized? | Return default value and log error (graceful degradation - matches earlier decisions) |
| 7 | Error Handling | Should conversion failures assert in debug or just log? | Assert in debug, log in release (fail-fast during development, graceful in production) |
| 8 | API Design | Should we have Try* variants (TryToInt) that return bool + out param? | No - current API sufficient (type queries like IsInt() allow checking before conversion) |
| 9 | Testing | Should we test with actual Python scripts calling C++ functions? | No - unit tests only (module-api feature already has integration tests covering the full flow) |
| 10 | Performance | Should we optimize repeated ToString() calls (caching)? | Yes - cache result in PythonObjectImpl (already in design - avoids repeated Python string conversions) |

## Decisions

| ID | Decision | Rationale | Status |
|----|----------|-----------|--------|
| FD-001 | Conversion failures return default values and log errors | Matches graceful degradation pattern; no exceptions; simpler than bool+out param | Proposed |
| FD-002 | Implicit type coercion follows Python rules | Natural for Python users; matches Python behavior (int("123"), float(42)) | Proposed |
| FD-003 | PythonObject lifetime managed automatically via reference counting | Python uses reference counting; pybind11::object handles this; caller doesn't manage memory | Proposed |
| FD-004 | Start with primitive types only (int, float, bool, string) | Simpler initial implementation; covers most CLI use cases; can enhance later | Proposed |
| FD-005 | ToString() returns const char* to internal cached string | Avoids memory management complexity; valid until PythonObject destroyed | Proposed |

## Status
**Done** - Implemented and tested. Core functionality working.

`Done` - Implemented
