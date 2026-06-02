#pragma once

#include <DiaCore/Metadata/DescriptionRegistry.h>

#define DIA_DESCRIBE(Type, Description) \
	namespace { \
		struct Type##_Describer { \
			Type##_Describer() { \
				Dia::Core::Metadata::DescriptionRegistry::Register(Type::kUniqueId, Description); \
			} \
		}; \
		static Type##_Describer Type##_describerInstance; \
	}
