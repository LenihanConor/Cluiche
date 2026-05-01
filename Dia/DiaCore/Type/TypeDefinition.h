#ifndef DIA_TYPE_DEFINITION_H
#define DIA_TYPE_DEFINITION_H


#include "DiaCore/Type/BasicTypeDefines.h"
#include "DiaCore/Memory/Memory.h"
#include "DiaCore/Containers/LinkList/LinkListC.h"
#include "DiaCore/Type/TypeMember.h"
#include "DiaCore/Containers/BitFlag/BitArray8.h"

namespace Dia { namespace Core { class CRC; } }

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeVariable;
			class TypeParameterInput;

			class TypeDefinition: public TypeMember
			{
			public:
				typedef Containers::LinkListC<TypeVariable*> VariableLinkList;
				typedef Containers::LinkListNode<TypeVariable*> VariableLinkListNode;

				TypeDefinition();
				TypeDefinition(const char* name, unsigned int size, bool isPolymorphic, const TypeParameterInput& input);

				const unsigned int GetUniqueCRC()const;		
				
				bool IsPolymorphicType()const;

				unsigned int NumFields()const;
				const VariableLinkList& GetVariables()const;

			private:
				enum BitFlagIndex
				{
					kPolymorphic = 0
				};
				
				void AppendBaseVariable(VariableLinkList& variableToAppendTo);

				unsigned int	mUniqueCRC;					// CRC to uniquely identify the class and its data members
						
				const TypeDefinition* mBaseType;
				VariableLinkList mVariables;

				unsigned int mSize;	
				BitArray8 mFlags;
			};
		}
	}
}

#endif // DIA_ASSERT