#include "DiaCore/Type/typetraits.h"
#include "DiaCore/Type/TypeInstance.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class AttributeSearchFunctor
			{
			public:
				bool Equal(const TypeVariableAttributes* object1, const CRC& object2)const
				{
					return (object1->GetUniqueID() == object2);
				}
			};

			//------------------------------------------------------------------------------------
			//	TypeVariable
			//------------------------------------------------------------------------------------
			template<typename T> inline
			TypeVariable::TypeVariable(T* field, const char* name, unsigned int size, unsigned int offset, unsigned int numberOfElements )
				: TypeMember(name)
				, mSize(size)
				, mOffsetFromParent(offset)
				, mNumberOfElements(numberOfElements)
				, mFlags()
			{
				bool isPointer = IsPointer<T>::value;
				bool isPointerArithmetic = IsArithmeticPointer<T>::value;
				bool isArithmetic = IsArithmetic<T>::value;
				bool isClass = IsClass<T>::value;
				bool isArray = numberOfElements > 1;

				DIA_ASSERT(!IsReference<T>::value, "Do not support references, use pointer");

				if (isPointer)
				{
					if (isPointerArithmetic)
					{
						mFlags.ToggleBit(kIsPointerArithmeticType);
					}
					else
					{
						mFlags.ToggleBit(kIsPointerClassType);
					}
				}					
				
				if (isArithmetic)
				{
					mFlags.ToggleBit( kIsArithmetic );				
				}				
				
				if (isClass)
				{
					mFlags.ToggleBit( kIsClassType );			
				}

				if (isArray)
				{
					mFlags.ToggleBit( kIsArrayType );
				}
			
				typedef MetaIf< IsPointer<T>::value, TypeVariableDataPointerClass, TypeVariableDataClass >::Type TypedefClass;
				typedef MetaIf< MetaOr< IsArithmeticPointer<T>::value, IsArithmetic<T>::value >::value, TypeVariableDataArithmetic, TypedefClass >::Type TypeVariableDataTypedef;

				mSpecificData = DIA_NEW(TypeVariableDataTypedef(field));
			}
			
			//------------------------------------------------------------------------------------
			template<class AttributeClass> 
			bool TypeVariable::HasAttribute()const
			{
				AttributeSearchFunctor searchFuntor;
				
				return (mAttributes.FindConst(AttributeClass::GetStaticUniqueID(), searchFuntor) != NULL);
			}
			
			//------------------------------------------------------------------------------------
			template<class AttributeClass> 
			AttributeClass* TypeVariable::GetAttribute()
			{
				AttributeSearchFunctor searchFuntor;

				AttributeLinkListNode* nodePtr = mAttributes.Find(AttributeClass::GetStaticUniqueID(), searchFuntor);
				
				if (nodePtr == NULL)
				{
					return NULL;
				}

				TypeVariableAttributes* attribute = nodePtr->GetPayload();

				return (static_cast<AttributeClass*>(attribute));
			}

			//------------------------------------------------------------------------------------
			template<class AttributeClass> 
			const AttributeClass* TypeVariable::GetAttributeConst()const
			{
				AttributeSearchFunctor searchFuntor;

				const AttributeLinkListNode* nodePtr = mAttributes.FindConst(AttributeClass::GetStaticUniqueID(), searchFuntor);

				if (nodePtr == NULL)
				{
					return NULL;
				}

				const TypeVariableAttributes* attribute =  nodePtr->GetPayloadConst();

				return (static_cast<const AttributeClass*>(attribute));
			}

			//------------------------------------------------------------------------------------
			template<class T>
			T TypeVariable::GetArithmeticValue( const TypeInstance& instance, unsigned int element )const
			{
				T value = 0;
				
				bool ableToConvertVariableIntoArthimetic = false;
				
				char offset = (GetOffsetFromParent(element) / sizeof (char));

				if ( IsArithmeticType() )
				{
					ableToConvertVariableIntoArthimetic = true;
					
					const char* pointeeAsType = static_cast<const char*>(instance.Pointee());
					value = *reinterpret_cast<const T*>(offset + pointeeAsType);
				}

				if ( !ableToConvertVariableIntoArthimetic && IsPointerArthmeticType() && HasAttribute<TypeVariableAttributesPointerAsObject>() )
				{
					ableToConvertVariableIntoArthimetic = true;

					const char* pointeeAsType = reinterpret_cast<const char*>(instance.Pointee());	
					const unsigned int* ptrAsInt = reinterpret_cast<const unsigned int*>(offset + pointeeAsType);

					unsigned int address = *ptrAsInt;
					value = *reinterpret_cast<T*>(address);
				}

				DIA_ASSERT( ableToConvertVariableIntoArthimetic, "Could not turn %s in %s into an arthmetic value", this->GetName(), instance.GetTypeDescriptor()->GetName() );

				return value;
			}

			//------------------------------------------------------------------------------------
			template<class T>
			void TypeVariable::SetArithmeticValue( const T& value, TypeInstance& instance, unsigned int element, bool isArthmeticPointer )const
			{
				char* pointeeAsType = reinterpret_cast<char*>(instance.Pointee());

				if ( isArthmeticPointer )
				{
					T** pointeeValue = NULL;
					pointeeValue = reinterpret_cast<T**>(((GetOffsetFromParent(element) / sizeof (char)) + pointeeAsType));
					**pointeeValue = value;
				}
				else
				{
					T* pointeeValue = NULL;
					pointeeValue = reinterpret_cast<T*>(((GetOffsetFromParent(element) / sizeof (char)) + pointeeAsType));
					*pointeeValue = value;
				}
			}
		}
	}
}