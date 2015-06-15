#ifndef DIA_TYPE_MACROS_DEFINITION_H
#define DIA_TYPE_MACROS_H

#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeParameterInput.h"

// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_DEFINITION( className )\
	Dia::Core::Types::TypeDefinition* className::sType = DIA_NEW( Dia::Core::Types::TypeDefinition( #className, sizeof(className), DIA_IS_POLYMORPHIC(className), className::TypeCreationalInput() ) );\
	Dia::Core::Types::TypeDefinition& className::GetType() {return *sType;}\
	const Dia::Core::Types::TypeDefinition& className::GetTypeConst()const {return *sType;}\
	Dia::Core::Types::TypeInstance className::CreateTypeInstance(){ return (Dia::Core::Types::TypeInstance(className::sType, this)); }\
	Dia::Core::Types::TypeInstance className::CreateTypeInstanceConst()const{ return (Dia::Core::Types::TypeInstance(className::sType, this)); }\
	Dia::Core::Types::TypeParameterInput& className::TypeCreationalInput()\
	{\
		typedef className MyType;\
		static MyType foo;\
		static Dia::Core::Types::TypeParameterInput typeInput;\
		Dia::Core::Types::TypeVariable* lastVariable = NULL;\

// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_TEMPLATE_DEFINITION( className, templateDescription) \
	template templateDescription Dia::Core::Types::TypeDefinition* className::sType = DIA_NEW( Dia::Core::Types::TypeDefinition( #className, sizeof(className), DIA_IS_POLYMORPHIC(className), className::TypeCreationalInput() ) );\
	template templateDescription const Dia::Core::Types::TypeDefinition& className::GetType() {return *sType;}\
	template templateDescription Dia::Core::Types::TypeInstance className::CreateTypeInstance()const{ return (Dia::Core::Types::TypeInstance(className::sType, this)); }\
	template templateDescription Dia::Core::Types::TypeParameterInput& className::TypeCreationalInput()\
	{ \
		typedef className MyType;\
		static MyType foo;\
		static Dia::Core::Types::TypeParameterInput typeInput;\
		Dia::Core::Types::TypeVariable* lastVariable = NULL;\

// --------------------------------------------------------------------------------------------------------------------
#define DIA_TYPE_DEFINITION_END()\
		return typeInput;\
	}\

#define DIA_TYPE_BASE_TYPE( baseClassName )\
	typeInput.SetBaseType( baseClassName::sType );\

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

#endif // DIA_TYPE_MACROS_H