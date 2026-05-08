#include "DiaCore/Type/typetraits.h"


namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//------------------------------------------------------------------------------------
			//	TypeVariableDataArithmetic
			//------------------------------------------------------------------------------------
			template<class T> 
			TypeVariableDataArithmetic::TypeVariableDataArithmetic(T* field)
				: TypeVariableDataBase()
			{
				bool isUnsigned = IsUnsigned<T>::value || IsUnsignedPointer<T>::value;
				bool isBool = IsBool<T>::value || IsBoolPointer<T>::value;
				bool isChar = IsChar<T>::value || IsCharPointer<T>::value;
				bool isShort = IsShort<T>::value || IsShortPointer<T>::value;
				bool isInt = IsInt<T>::value || IsIntPointer<T>::value;
				bool isLong = IsLong<T>::value || IsLongPointer<T>::value;
				bool isLongLong = IsLongLong<T>::value || IsLongLongPointer<T>::value;
				bool isFloat = IsFloat<T>::value || IsFloatPointer<T>::value;
				bool isDouble = IsDouble<T>::value || IsDoublePointer<T>::value;

				mIsUnsigned = isUnsigned;

				if (isBool)
					mFlags.ToggleBit( kIsArithmeticBool );
				else if (isChar)
					mFlags.ToggleBit( kIsArithmeticChar );
				else if (isShort)
					mFlags.ToggleBit( kIsArithmeticShort );
				else if (isInt)
					mFlags.ToggleBit( kIsArithmeticInt );
				else if (isLong)
					mFlags.ToggleBit( kIsArithmeticLong );
				else if (isLongLong)
					mFlags.ToggleBit( kIsArithmeticLongLong );
				else if (isFloat)
					mFlags.ToggleBit( kIsArithmeticFloat );
				else if (isDouble)
					mFlags.ToggleBit( kIsArithmeticDouble );
			}

			//------------------------------------------------------------------------------------
			//	TypeVariableDataClass
			//------------------------------------------------------------------------------------
			template<class T> 
			TypeVariableDataClass::TypeVariableDataClass(T* field)
				: TypeVariableDataBase()
				, mClassDefinition(nullptr)
			{
				mClassDefinition = &field->GetType();

				DIA_ASSERT(mClassDefinition, "Could not find type class, potentially it has not been initialized yet. Add an include in file to ensure order");
			}

			//------------------------------------------------------------------------------------
			//	TypeVariableDataPointerClass
			//------------------------------------------------------------------------------------
			template<class T> 
			TypeVariableDataPointerClass::TypeVariableDataPointerClass(T* field)
				: TypeVariableDataBase()
				, mClassDefinition(nullptr)
			{
				mClassDefinition = &((*field)->GetType());	

				DIA_ASSERT(mClassDefinition, "Could not find type class, potentially it has not been initialized yet. Add an include in file to ensure order");
			}
		}
	}
}