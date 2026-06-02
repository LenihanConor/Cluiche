#pragma once

#include <DiaCore/Metadata/DescriptionRegistry.h>

// DIA_DESCRIBE_CORE(IdExpr, Description)
// Registers into DescriptionRegistry only. Safe to use from any layer.
#define DIA_DESCRIBE_CORE_IMPL(counter, IdExpr, Description) \
	namespace { \
		struct _DiaDescriber_##counter { \
			_DiaDescriber_##counter() { \
				Dia::Core::Metadata::DescriptionRegistry::Register(IdExpr, Description); \
			} \
		}; \
		static _DiaDescriber_##counter _s_diaDescriber_##counter; \
	}

#define DIA_DESCRIBE_CORE(IdExpr, Description) DIA_DESCRIBE_CORE_IMPL(__COUNTER__, IdExpr, Description)

// DIA_COMPONENT_DESCRIBE(ClassName, Description)
// Convenience alias for components — uses kTypeId.
#define DIA_COMPONENT_DESCRIBE(ClassName, Description) \
	DIA_DESCRIBE_CORE(ClassName::kTypeId, Description)
