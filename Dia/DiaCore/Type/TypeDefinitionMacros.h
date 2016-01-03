#pragma once

#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeParameterInput.h"

// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_DEFINITION( className )\
	Dia::Core::Types::TypeDefinition* className::sType;\
	Dia::Core::Types::TypeDefinition* className::GetTypeStatic(){if (className::sType == nullptr){className::sType = DIA_NEW( Dia::Core::Types::TypeDefinition( #className, sizeof(className), DIA_IS_POLYMORPHIC(className), className::TypeCreationalInput() ) );} return className::sType;}\
	Dia::Core::Types::TypeDefinition& className::GetType() { return *className::GetTypeStatic(); }\
	const Dia::Core::Types::TypeDefinition& className::GetTypeConst()const {return *className::GetTypeStatic(); }\
	Dia::Core::Types::TypeInstance className::CreateTypeInstance(){ return (Dia::Core::Types::TypeInstance(className::GetTypeStatic(), this)); }\
	Dia::Core::Types::TypeInstance className::CreateTypeInstanceConst()const{ return (Dia::Core::Types::TypeInstance(className::GetTypeStatic(), this)); }\
	Dia::Core::Types::TypeParameterInput& className::TypeCreationalInput()\
	{\
		typedef className MyType;\
		static MyType foo;\
		static Dia::Core::Types::TypeParameterInput typeInput;\
		Dia::Core::Types::TypeVariable* lastVariable = NULL;\

// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_DEFINITION_END()\
		return typeInput;\
	}\

// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_BASE_TYPE( baseClassName )\
	typeInput.SetBaseType( baseClassName::GetTypeStatic() );\

// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_ADD_VARIABLE( variableName, variable )\
	{\
		static Dia::Core::Types::TypeVariable newVariable(&foo.variable, variableName, sizeof(foo.variable), (reinterpret_cast<char*>(&foo.variable) - reinterpret_cast<char*>(&foo)), 1);\
		static Dia::Core::Types::TypeDefinition::VariableLinkListNode newVariableNode(&newVariable);\
		bool succesAddNode = Dia::Core::Types::TypeDefinition::VariableLinkList::AddNodeToList(typeInput.GetVariables(), &newVariableNode);\
		lastVariable = newVariableNode.GetPayload();\
	}\
// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_ADD_VARIABLE_ARRAY( variableName, variable, numberOfElements )\
	{\
		static Dia::Core::Types::TypeVariable newVariable(foo.variable, variableName, sizeof(foo.variable), (reinterpret_cast<char*>(&foo.variable) - reinterpret_cast<char*>(&foo)), numberOfElements);\
		static Dia::Core::Types::TypeDefinition::VariableLinkListNode newVariableNode(&newVariable);\
		bool succesAddNode = Dia::Core::Types::TypeDefinition::VariableLinkList::AddNodeToList(typeInput.GetVariables(), &newVariableNode);\
		lastVariable = newVariableNode.GetPayload();\
	}\

// --------------------------------------------------------------------------------------------------------------------
#define  DIA_TYPE_ADD_VARIABLE_ATTRIBUTE( attributeClass )\
	{\
		static attributeClass newAttribute;\
		static Dia::Core::Types::TypeVariable::AttributeLinkListNode newAttributeNode(&newAttribute);\
		DIA_ASSERT(lastVariable != NULL, "Last Varibale is NULL");\
		lastVariable->AddAttribute(&newAttributeNode);\
	}\

// --------------------------------------------------------------------------------------------------------------------
#define  DIA_TYPE_ADD_VARIABLE_ATTRIBUTE_PARAM_1( attributeClass, attribParam_1 )\
	{\
		static attributeClass newAttribute(attribParam_1);\
		static Dia::Core::Types::TypeVariable::AttributeLinkListNode newAttributeNode(&newAttribute);\
		DIA_ASSERT(lastVariable != NULL, "Last Varibale is NULL");\
		lastVariable->AddAttribute(&newAttributeNode);\
	}
