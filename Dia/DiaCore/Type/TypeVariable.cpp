#include "DiaCore/Type/TypeVariable.h"

#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Memory/Memory.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			TypeVariable::TypeVariable(){}	

			//------------------------------------------------------------------------------------
			TypeVariable::~TypeVariable()
			{
				if ( mSpecificData != NULL )
					DIA_DELETE( mSpecificData );
			}
			//------------------------------------------------------------------------------------
			unsigned int TypeVariable::GetSize()const							
			{ 
				return mSize; 
			}

			//------------------------------------------------------------------------------------
			unsigned int TypeVariable::GetOffsetFromParent(unsigned int element)const				
			{ 
				return mOffsetFromParent + (element * (mSize / GetNumberOfElements())); 
			}

			//------------------------------------------------------------------------------------
			unsigned int TypeVariable::GetNumberOfElements()const
			{
				return mNumberOfElements;
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticType()const								
			{ 
				return mFlags.GetBit(kIsArithmetic); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsClassType()const								
			{ 
				return mFlags.GetBit(kIsClassType); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsPointerType()const								
			{ 
				return mFlags.GetBit(kIsPointerArithmeticType) || mFlags.GetBit(kIsPointerClassType); 
			}
			
			//------------------------------------------------------------------------------------
			bool TypeVariable::IsPointerArthmeticType()const
			{
				return mFlags.GetBit(kIsPointerArithmeticType); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsPointerClassType()const
			{
				return mFlags.GetBit(kIsPointerClassType); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArrayType()const
			{
				return mFlags.GetBit(kIsArrayType); 
			}

			//------------------------------------------------------------------------------------
			const Dia::Core::Types::TypeDefinition* TypeVariable::GetClassDefinition()const
			{
				DIA_ASSERT( IsClassType() || IsPointerClassType(), "%s Must be a class to return it", GetName() );
				
				if (IsPointerClassType())
					return static_cast<TypeVariableDataPointerClass*>(mSpecificData)->GetClassDefinition(); 
				
				return static_cast<TypeVariableDataClass*>(mSpecificData)->GetClassDefinition(); 
			}
			
			//------------------------------------------------------------------------------------
			char* TypeVariable::GetClassPointee(TypeInstance& instance, unsigned int element)const
			{
				DIA_ASSERT( IsClassType() || IsPointerClassType(), "%s Must be a class to return it", GetName() );

				char* value = NULL;
				if (IsPointerClassType())
				{
					value = GetVariablePointer(instance, element);
				}
				else
				{
					char* pointeeAsType = reinterpret_cast<char*>(instance.Pointee());
					value = ((GetOffsetFromParent(element) / sizeof (char)) + pointeeAsType);
				}

				return value;
			}

			//------------------------------------------------------------------------------------
			const char* TypeVariable::GetClassPointee(const TypeInstance& instance, unsigned int element)const
			{
				DIA_ASSERT( IsClassType() || IsPointerClassType(), "%s Must be a class to return it", GetName() );

				const char* value = NULL;
				if (IsPointerClassType())
				{
					value = GetVariablePointer(instance, element);
				}
				else
				{
					const char* pointeeAsType = reinterpret_cast<const char*>(instance.Pointee());
					value = ((GetOffsetFromParent(element) / sizeof (char)) + pointeeAsType);
				}

				return value;
			}
			
			//------------------------------------------------------------------------------------
			unsigned int TypeVariable::GetVariableAddress(TypeInstance& instance, unsigned int element)const
			{
				char* pointeeAsType = reinterpret_cast<char*>(instance.Pointee());	
				unsigned int* ptrAsInt = reinterpret_cast<unsigned int*>((GetOffsetFromParent(element) / sizeof (char)) + pointeeAsType);

				unsigned int address = *ptrAsInt;

				return address;
			}

			//------------------------------------------------------------------------------------
			unsigned int TypeVariable::GetVariableAddress(const TypeInstance& instance, unsigned int element)const
			{
				const char* pointeeAsType = reinterpret_cast<const char*>(instance.Pointee());	
				const unsigned int* ptrAsInt = reinterpret_cast<const unsigned int*>((GetOffsetFromParent(element) / sizeof (char)) + pointeeAsType);

				unsigned int address = *ptrAsInt;

				return address;
			}

			//------------------------------------------------------------------------------------
			char* TypeVariable::GetVariablePointer(TypeInstance& instance, unsigned int element)const
			{
				unsigned int address = GetVariableAddress(instance, element);
				char* value = reinterpret_cast<char*>(address);

				return value;
			}

			//------------------------------------------------------------------------------------
			const char* TypeVariable::GetVariablePointer(const TypeInstance& instance, unsigned int element)const
			{
				unsigned int address = GetVariableAddress(instance, element);
				const char* value = reinterpret_cast<const char*>(address);

				return value;
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticUnsigned()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticUnsigned(); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticBool()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticBool(); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticChar()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticChar(); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticShort()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticShort(); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticInt()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticInt(); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticLong()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticLong();  
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticLongLong()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticLongLong(); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticFloat()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticFloat(); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariable::IsArithmeticDouble()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->IsArithmeticDouble(); 
			}
			
			//------------------------------------------------------------------------------------
			TypeVariableDataArithmetic::ArithmeticType TypeVariable::GetArithmeticType()const
			{
				DIA_ASSERT(IsArithmeticType() || IsPointerArthmeticType(), "%s is not arithmetic type", GetName());
				return static_cast<TypeVariableDataArithmetic*>(mSpecificData)->GetArithmeticType(); 
			}

			//------------------------------------------------------------------------------------
			void TypeVariable::AddAttribute(AttributeLinkListNode* newNode)
			{
				newNode->GetPayload()->AssignedTo(*this);
				AttributeLinkList::AddNodeToList(mAttributes, newNode);
			}
		}
	}
}