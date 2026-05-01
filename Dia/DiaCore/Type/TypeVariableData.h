#ifndef DIA_TYPE_VARIABLE_DATA_H
#define DIA_TYPE_VARIABLE_DATA_H

#include "DiaCore/Type/BasicTypeDefines.h"
#include "DiaCore/Containers/BitFlag/BitArray8.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeDefinition;

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableDataBase
			//---------------------------------------------------------------------------------------------------------
			class TypeVariableDataBase
			{
			public:
				TypeVariableDataBase(){}
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableDataArithmetic
			//---------------------------------------------------------------------------------------------------------
			class TypeVariableDataArithmetic : public TypeVariableDataBase
			{
			public:
				enum ArithmeticType
				{
					kIsArithmeticChar = 0, 
					kIsArithmeticShort,
					kIsArithmeticInt,
					kIsArithmeticLong,
					kIsArithmeticLongLong,
					kIsArithmeticBool,
					kIsArithmeticFloat,
					kIsArithmeticDouble,
					kNumOfArthmeticTypes
				};

				template<class T>
				TypeVariableDataArithmetic(T* field);
				
				bool IsArithmeticUnsigned()const;
				bool IsArithmeticBool()const;
				bool IsArithmeticChar()const;
				bool IsArithmeticShort()const;
				bool IsArithmeticInt()const;
				bool IsArithmeticLong()const;
				bool IsArithmeticLongLong()const;
				bool IsArithmeticFloat()const;
				bool IsArithmeticDouble()const;
				
				const char* GetTypeAsString()const;

				ArithmeticType GetArithmeticType()const;
			private:
				bool mIsUnsigned;
				BitArray8 mFlags;
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableDataClass
			//---------------------------------------------------------------------------------------------------------
			class TypeVariableDataClass : public TypeVariableDataBase
			{
			public:			
				template<class T>
				TypeVariableDataClass(T* field);
				
				const Dia::Core::Types::TypeDefinition* GetClassDefinition()const;
			private:
				const Dia::Core::Types::TypeDefinition* mClassDefinition;
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableDataPointerClass
			//---------------------------------------------------------------------------------------------------------
			class TypeVariableDataPointerClass : public TypeVariableDataBase
			{
			public:			
				template<class T>
				TypeVariableDataPointerClass(T* field);

				const Dia::Core::Types::TypeDefinition* GetClassDefinition()const;
			private:
				const Dia::Core::Types::TypeDefinition* mClassDefinition;
			};
		}
	}
}

#include "DiaCore/Type/TypeVariableData.inl"

#endif // DIA_ASSERT