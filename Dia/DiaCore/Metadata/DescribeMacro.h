#pragma once

#include <DiaCore/Metadata/DescriptionRegistry.h>
#include <DiaApplicationFlow/TypeRegistry.h>

// DIA_DESCRIBE(IdExpr, Description)
// Place in the .cpp file next to DIA_MODULE or the component registration.
// IdExpr is the StringCRC ID — typically MyModule_::kTypeId or MyComponent::kUniqueId.
// Registers into both DescriptionRegistry (generic) and TypeRegistry (module editor).
// Example:
//   DIA_DESCRIBE(KernelModule_::kTypeId, "Manages window, GL context, and render loop.");
#define DIA_DESCRIBE_IMPL(counter, IdExpr, Description) \
	namespace { \
		struct _DiaDescriber_##counter { \
			_DiaDescriber_##counter() { \
				Dia::Core::Metadata::DescriptionRegistry::Register(IdExpr, Description); \
				Dia::ApplicationFlow::TypeRegistry::Global().SetDescription(IdExpr, Description); \
			} \
		}; \
		static _DiaDescriber_##counter _s_diaDescriber_##counter; \
	}

#define DIA_DESCRIBE(IdExpr, Description) DIA_DESCRIBE_IMPL(__COUNTER__, IdExpr, Description)
