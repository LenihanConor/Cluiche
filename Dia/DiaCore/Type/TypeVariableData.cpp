#include "DiaCore/Type/TypeVariableData.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//---------------------------------------------------------------------------------------------------------
			// TypeVariableDataArithmetic
			//---------------------------------------------------------------------------------------------------------

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticUnsigned()const
			{
				return mIsUnsigned; 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticBool()const
			{
				return mFlags.GetBit(kIsArithmeticBool); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticChar()const
			{
				return mFlags.GetBit(kIsArithmeticChar); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticShort()const
			{
				return mFlags.GetBit(kIsArithmeticShort); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticInt()const
			{
				return mFlags.GetBit(kIsArithmeticInt); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticLong()const
			{
				return mFlags.GetBit(kIsArithmeticLong); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticLongLong()const
			{
				return mFlags.GetBit(kIsArithmeticLongLong); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticFloat()const
			{
				return mFlags.GetBit(kIsArithmeticFloat); 
			}

			//------------------------------------------------------------------------------------
			bool TypeVariableDataArithmetic::IsArithmeticDouble()const
			{
				return mFlags.GetBit(kIsArithmeticDouble); 
			}

			//------------------------------------------------------------------------------------
			TypeVariableDataArithmetic::ArithmeticType TypeVariableDataArithmetic::GetArithmeticType()const
			{
				bool isBool = IsArithmeticBool();
				bool isChar = IsArithmeticChar();
				bool isShort = IsArithmeticShort();
				bool isInt = IsArithmeticInt();
				bool isLong = IsArithmeticLong();
				bool isLongLong = IsArithmeticLongLong();
				bool isFloat = IsArithmeticFloat();
				bool isDouble = IsArithmeticDouble();

				ArithmeticType result = kIsArithmeticChar;

				if (isBool)
				{
					result = kIsArithmeticBool;
				}
				else if (isChar)
				{
					result = kIsArithmeticChar;
				}
				else if (isShort)
				{
					result = kIsArithmeticShort;
				}
				else if (isInt)
				{
					result = kIsArithmeticInt;
				}
				else if (isLong)
				{
					result = kIsArithmeticLong;
				}
				else if (isLongLong)
				{
					result = kIsArithmeticLongLong;
				}
				else if (isFloat)
				{
					result = kIsArithmeticFloat;
				}
				else if (isDouble)
				{
					result = kIsArithmeticDouble;
				}

				return result; 
			}

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableDataClass
			//---------------------------------------------------------------------------------------------------------

			const Dia::Core::Types::TypeDefinition* TypeVariableDataClass::GetClassDefinition()const
			{
				return mClassDefinition;
			}

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableDataPointerClass
			//---------------------------------------------------------------------------------------------------------

			const Dia::Core::Types::TypeDefinition* TypeVariableDataPointerClass::GetClassDefinition()const
			{
				return mClassDefinition;
			}
		}
	}
}