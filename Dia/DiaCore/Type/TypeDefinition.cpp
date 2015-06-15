#include "DiaCore/Type/TypeDefinition.h"

#include "DiaCore/Type/TypeMember.h"
#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeFacade.h"
#include "DiaCore/Type/TypeParameterInput.h"
#include "DiaCore/Strings/String1024.h"
namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//------------------------------------------------------------------------------------
			//	TypeDefinition
			//------------------------------------------------------------------------------------
			class SortVariableByOffset
			{
			public:
				bool GreaterThen(const TypeVariable* object1, const TypeVariable* object2)const
				{
					return (object1->GetOffsetFromParent() > object2->GetOffsetFromParent());
				}
				bool LessThen(const TypeVariable* object1, const TypeVariable* object2)const
				{
					return (object1->GetOffsetFromParent() < object2->GetOffsetFromParent());
				}
				bool Equal(const TypeVariable* object1, const TypeVariable* object2)const
				{
					return (object1->GetOffsetFromParent() == object2->GetOffsetFromParent());
				}
			};


			//------------------------------------------------------------------------------------
			//	TypeDefinition
			//------------------------------------------------------------------------------------
			TypeDefinition::TypeDefinition(){}
		
			TypeDefinition::TypeDefinition(const char* name, unsigned int size, bool isPolymorphic, const TypeParameterInput& input)
				: TypeMember(name)
				, mBaseType(NULL)
				, mSize(size)
				, mFlags()
			{
				mFlags.SetBit( kPolymorphic, isPolymorphic );
				mVariables = input.GetVariables();

				unsigned int runningCRC = 0;

				runningCRC = GetHashID();

				unsigned numberOfVariables = 0;

				// If it has a base class then append it to the crc
				mBaseType = input.GetBaseType();
				if (mBaseType != NULL)
				{
					DIA_ASSERT_CONDITIONAL( IsPolymorphicType(), mBaseType->IsPolymorphicType(), "Type is inherited from non-polymorhic class");
					DIA_ASSERT_CONDITIONAL( !IsPolymorphicType(), !mBaseType->IsPolymorphicType(), "Type is inherited from polymorhic class");
					
					unsigned int baseCRC = mBaseType->GetHashID();

					runningCRC /= 2;
					baseCRC /= 2;
					runningCRC += baseCRC;
					
					AppendBaseVariable(mVariables);
				}

				SortVariableByOffset sortVariableByOffset;
				mVariables.Sort(sortVariableByOffset);

				// If it has variables  then append it to the crc
				const TypeDefinition::VariableLinkListNode* nextVariable = mVariables.Head();
				while (nextVariable != NULL)
				{
					numberOfVariables++;
					const TypeVariable& typeVariable = *nextVariable->GetPayloadConst();
					unsigned int variableCRC = typeVariable.GetHashID();
					variableCRC += (typeVariable.GetSize() + typeVariable.GetOffsetFromParent()) * numberOfVariables;

					runningCRC /= 2;
					variableCRC /= 2;
					runningCRC += variableCRC;
					
					nextVariable = nextVariable->GetNextConst();
				}

				mUniqueCRC = CRC(runningCRC);
				
				GetTypeFacade().Registry().Add(this);
			}
		
			const CRC& TypeDefinition::GetUniqueCRC()const 
			{ 
				return mUniqueCRC; 
			}

			bool TypeDefinition::IsPolymorphicType()const
			{
				return mFlags.GetBit(kPolymorphic);
			}

			unsigned int TypeDefinition::NumFields()const
			{
				return mVariables.Size();
			}

			const TypeDefinition::VariableLinkList& TypeDefinition::GetVariables()const
			{
				return mVariables;
			}

			void TypeDefinition::AppendBaseVariable(VariableLinkList& variableToAppendTo)
			{
				class SameAsOperator
				{
				public:
					bool Equal(const TypeVariable* object1, const TypeVariable* object2)const
					{
						return (object1->GetNameCRC() == (object2->GetNameCRC()));
					}
				};

				SameAsOperator fuctor;

				if (mBaseType != NULL)
				{
					TypeDefinition* unConstBaseType = const_cast<TypeDefinition*>(mBaseType);
					
#ifdef DEBUG
					TypeDefinition::VariableLinkListNode* currentNode = unConstBaseType->mVariables.Head();
					while (currentNode != NULL)
					{
						DIA_ASSERT(!variableToAppendTo.Contains(currentNode->GetPayloadConst(), fuctor), "Name collision %s, in %s", currentNode->GetPayload()->GetName(), GetName());
						
						currentNode = currentNode->GetNext();
					}
#endif					
					variableToAppendTo.AddNodeToHead(unConstBaseType->mVariables.Head());
					unConstBaseType->AppendBaseVariable(variableToAppendTo);
				}			
			}
		}
	}
}