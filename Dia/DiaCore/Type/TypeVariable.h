#ifndef DIA_TYPE_FIELD_H
#define DIA_TYPE_FIELD_H

#include "DiaCore/Type/TypeMember.h"
#include "DiaCore/Type/BasicTypeDefines.h"
#include "DiaCore/Type/TypeVariableAttributes.h"
#include "DiaCore/Type/TypeVariableData.h"
#include "DiaCore/Containers/BitFlag/BitArray8.h"
#include "DiaCore/Containers/BitFlag/BitArray16.h"
#include "DiaCore/Containers/LinkList/LinkListC.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeDefinition;
			class TypeInstance;
			class TypeVariableDataBase;

			//---------------------------------------------------------------------------------------------------------
			// TypeVariable
			//---------------------------------------------------------------------------------------------------------
			class TypeVariable : public TypeMember
			{
			public:
				typedef Containers::LinkListC<TypeVariableAttributes*> AttributeLinkList;
				typedef Containers::LinkListNode<TypeVariableAttributes*> AttributeLinkListNode;
	
				TypeVariable();
				~TypeVariable();

				template<typename T> TypeVariable(T* field, const char* name, unsigned int size, unsigned int offset, unsigned int numberOfElements );
				
				unsigned int GetSize()const;
				unsigned int GetOffsetFromParent(unsigned int element = 0)const;
				unsigned int GetNumberOfElements()const;

				bool IsArithmeticType()const;				
				bool IsClassType()const;
				bool IsPointerType()const;
				bool IsPointerArthmeticType()const;
				bool IsPointerClassType()const;
				bool IsArrayType()const;

				// Arithmetic Interface
				bool IsArithmeticUnsigned()const;
				bool IsArithmeticBool()const;
				bool IsArithmeticChar()const;
				bool IsArithmeticShort()const;
				bool IsArithmeticInt()const;
				bool IsArithmeticLong()const;
				bool IsArithmeticLongLong()const;
				bool IsArithmeticFloat()const;
				bool IsArithmeticDouble()const;
				TypeVariableDataArithmetic::ArithmeticType GetArithmeticType()const;

				template<class T>
				T GetArithmeticValue( const TypeInstance& instance, unsigned int element )const;
				
				template<class T>
				void SetArithmeticValue(const T& value, TypeInstance& instance, unsigned int element, bool isArthmeticPointer )const;
				
				// Class Interface	
				const Dia::Core::Types::TypeDefinition* GetClassDefinition()const;
				char* GetClassPointee(TypeInstance& instance, unsigned int element)const;
				const char* GetClassPointee(const TypeInstance& instance, unsigned int element)const;

				// Pointer Interface
				unsigned int GetVariableAddress(TypeInstance& instance, unsigned int element)const;
				unsigned int GetVariableAddress(const TypeInstance& instance, unsigned int element)const;

				char* GetVariablePointer(TypeInstance& instance, unsigned int element)const;
				const char* GetVariablePointer(const TypeInstance& instance, unsigned int element)const;

				// Attribute Interface
				void AddAttribute(AttributeLinkListNode* newNode);
				template<class AttributeClass> bool HasAttribute()const;
				template<class AttributeClass> AttributeClass* GetAttribute();
				template<class AttributeClass> const AttributeClass* GetAttributeConst()const;

			private:
				enum BitFlagIndex
				{
					kIsArithmetic = 0,
					kIsPointerArithmeticType,
					kIsClassType,
					kIsPointerClassType,
					kIsArrayType,
				};
				
				unsigned int mSize;							// Size of the object being created
				unsigned int mOffsetFromParent;				// Where in the parent object should this variable be assigned
				unsigned int mNumberOfElements;				// If this is an array this will be greater then 1
				BitArray8 mFlags;
				TypeVariableDataBase* mSpecificData;
				AttributeLinkList mAttributes;
			};
		}
	}
}

#include "DiaCore/Type/TypeVariable.inl"

#endif // DIA_ASSERT