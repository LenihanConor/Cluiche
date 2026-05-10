#pragma once
#include <DiaApplicationFlow/TypeRegistry.h>

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
