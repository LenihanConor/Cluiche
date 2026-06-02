#pragma once

#include <DiaCore/Metadata/DescribeMacro.h>
#include <DiaApplicationFlow/TypeRegistry.h>

// DIA_DESCRIBE(IdExpr, Description)
// Place in the .cpp file next to DIA_MODULE.
// Registers into both DescriptionRegistry and TypeRegistry so the editor can query it.
// IdExpr is typically MyModule_::kTypeId.
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
