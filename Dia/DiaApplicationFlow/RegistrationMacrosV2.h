#pragma once
#include <DiaApplicationFlow/TypeRegistry.h>
#include <DiaApplicationFlow/Streams/StreamTypeRegistry.h>

// ---------------------------------------------------------------------------
// DIA_MODULE(ClassName)
//
// One-liner registration macro.  Place in the .cpp file of each Module
// subclass — NOT in the header, to avoid ODR violations.
//
// Requirements:
//   - ClassName must derive from Dia::ApplicationFlow::Module
//   - ClassName must declare:
//       static constexpr Dia::Core::StringCRC kTypeId{"TypeName"};
//   - ClassName must have a constructor accepting (const Dia::Core::StringCRC& instanceId)
//
// Example:
//   // MyModule.cpp
//   #include "MyModule.h"
//   #include <DiaApplicationFlow/RegistrationMacrosV2.h>
//   DIA_MODULE(MyModule);
// ---------------------------------------------------------------------------
#define DIA_MODULE(ClassName) \
    static Dia::ApplicationFlow::ModuleRegistration<ClassName> \
        s_reg_##ClassName { ClassName::kTypeId }

// ---------------------------------------------------------------------------
// DIA_STREAM_TYPE(T)
//
// One-liner registration macro for event/frame stream payload types.
// Place in the .cpp file of the owning module (NOT in a header) to
// avoid ODR violations — exactly one registration per type per process.
//
// T must be a complete type at the point of the macro.
// The StringCRC type ID is derived from the stringified type name (#T).
//
// Example:
//   // MyEvents.cpp
//   #include "MyEvents.h"
//   #include <DiaApplicationFlow/RegistrationMacrosV2.h>
//   DIA_STREAM_TYPE(MyEvent);
//
// Forbidden inside TapCallback: allocation, logging, Send, I/O.
// ---------------------------------------------------------------------------
#define DIA_STREAM_TYPE(T) \
    static Dia::ApplicationFlow::StreamTypeRegistration<T> \
        s_streamtype_##T { Dia::Core::StringCRC(#T) }
