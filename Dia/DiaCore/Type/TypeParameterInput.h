#ifndef DIA_TYPE_PARAMETER_INPUT_H
#define DIA_TYPE_PARAMETER_INPUT_H

#include "DiaCore/Containers/LinkList/LinkListC.h"
#include "DiaCore/Type/TypeVariable.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeDefinition;

			class TypeParameterInput
			{
			public:
				TypeParameterInput(): mBaseType(nullptr) {};

				void SetBaseType(const TypeDefinition* baseType);
				const TypeDefinition* GetBaseType()const { return mBaseType; };

				Dia::Core::Containers::LinkListC<Types::TypeVariable*>& GetVariables() { return mVariables; };
				const Dia::Core::Containers::LinkListC<Types::TypeVariable*>& GetVariables()const { return mVariables; };

			private:
				const TypeDefinition* mBaseType;
				Dia::Core::Containers::LinkListC<Types::TypeVariable*> mVariables;
			};
		}
	}
}

#endif // DIA_ASSERT