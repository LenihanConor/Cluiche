#ifndef DIA_TYPE_MACROS_DECLARATION_H
#define DIA_TYPE_MACROS_DECLARATION_H

#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Type/TypeInstance.h"

// --------------------------------------------------------------------------------------------------------------------
#define __INTERNAL__DIA_TYPE_DECLARATION\
	friend class Dia::Core::Types::TypeDefinition;\
	friend class Dia::Core::Types::TypeDefinition;\
	static Dia::Core::Types::TypeDefinition* sType;\
	static Dia::Core::Types::TypeDefinition* GetTypeStatic();\
	static Dia::Core::Types::TypeParameterInput& TypeCreationalInput();\

#define DIA_TYPE_DECLARATION\
	__INTERNAL__DIA_TYPE_DECLARATION\
	Dia::Core::Types::TypeDefinition& GetType();\
	const Dia::Core::Types::TypeDefinition& GetTypeConst()const;\
	Dia::Core::Types::TypeInstance CreateTypeInstance();\
	Dia::Core::Types::TypeInstance CreateTypeInstanceConst()const;\

#define DIA_TYPE_POLYMORPHIC_DECLARATION\
	__INTERNAL__DIA_TYPE_DECLARATION\
	Dia::Core::Types::TypeDefinition& GetType();\
	const Dia::Core::Types::TypeDefinition& GetTypeConst()const;\
	Dia::Core::Types::TypeInstance CreateTypeInstance();\
	Dia::Core::Types::TypeInstance CreateTypeInstanceConst()const;\

#endif // DIA_TYPE_MACROS_H