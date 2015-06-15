#include "DiaCore/Type/TypeInstance.h"

#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/stringutils.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//------------------------------------------------------------------------------------
			//	TypeInstance
			//------------------------------------------------------------------------------------
			TypeInstance::TypeInstance()
				: mInstanceDefinition(NULL)
				, mPointee(NULL)
				, mPointeeConst(NULL)
			{}

			TypeInstance::TypeInstance(const TypeDefinition* typeDefinition, void* pointee)
				: mInstanceDefinition(typeDefinition)
				, mPointee(pointee)
				, mPointeeConst(pointee)
			{}

			TypeInstance::TypeInstance(const TypeDefinition* typeDefinition, const void* pointee)
				: mInstanceDefinition(typeDefinition)
				, mPointee(NULL)
				, mPointeeConst(pointee)
			{}

			const TypeDefinition* TypeInstance::GetTypeDescriptor() const
			{
				return mInstanceDefinition;
			}	

			void* TypeInstance::Pointee() 
			{
				DIA_ASSERT(mPointee, "Pointee is null");

				return mPointee;
			}

			const void* TypeInstance::Pointee() const
			{
				DIA_ASSERT(mPointeeConst, "Pointee is null");

				return mPointeeConst;
			}

			unsigned int TypeInstance::NumFields()const
			{
				return mInstanceDefinition->NumFields();
			}

			const TypeDefinition::VariableLinkList& TypeInstance::GetVariables()const
			{
				return mInstanceDefinition->GetVariables();
			}

			void TypeInstance::FindVariablePathFromPointerAddress( const unsigned int address, VariablePath& resultPath )const
			{
				bool foundPtr = FindVariablePathFromPointerAddressInternal(address, resultPath, 0, GetVariables());

				DIA_ASSERT_SUPPORT(Containers::String32 hexAddress);
				DIA_ASSERT_SUPPORT(StringConvertFromUInt(hexAddress.AsCStr(), address, 16));
				DIA_ASSERT(foundPtr, "Could not find ptr address %d in %s", hexAddress.AsCStr(), mInstanceDefinition->GetName());
			}

			bool TypeInstance::FindVariablePathFromPointerAddressInternal( const unsigned int address, VariablePath& resultPath, const unsigned int currentClassOffset,  const TypeDefinition::VariableLinkList& variables )const
			{
				const int pointeeBaseAddress = *(reinterpret_cast<const unsigned int*>(mPointeeConst));

				const TypeDefinition::VariableLinkListNode* currentNode = variables.HeadConst();
				while (currentNode)
				{
					const TypeVariable* currentTypeVariable = currentNode->GetPayloadConst();
				
					const char* pointeeAsType = reinterpret_cast<const char*>(mPointeeConst);
					const unsigned int variableAddress = reinterpret_cast<const unsigned int>((((currentTypeVariable->GetOffsetFromParent() + currentClassOffset) / sizeof (char)) + pointeeAsType));

					if ( variableAddress == address)
					{
						resultPath.Add(currentTypeVariable);
						return true;
					}

					if ( currentTypeVariable->IsClassType() )
					{
						const unsigned int startClassAddress = variableAddress;
						const unsigned int endClassAddress = variableAddress + currentTypeVariable->GetSize();

						if (address >= startClassAddress && address <= endClassAddress)
						{
							resultPath.Add(currentTypeVariable);
							return FindVariablePathFromPointerAddressInternal(address, resultPath, currentClassOffset + currentTypeVariable->GetOffsetFromParent(), currentTypeVariable->GetClassDefinition()->GetVariables());
						}
					}

					currentNode = currentNode->GetNextConst();
				}

				return false;
			}

			void TypeInstance::FindPointerAddressFromVariableCRCPath( const VariableCRCPath& pathCRC, unsigned int& addressResult )const
			{
				DIA_ASSERT( pathCRC[0] == mInstanceDefinition->GetNameCRC(), "Could not fixup ptr in %s", mInstanceDefinition->GetName());
			
				bool found = FindPointerAddressFromVariableCRCPathInternal( pathCRC, addressResult, 1, 0, GetVariables() );
			}

			bool TypeInstance::FindPointerAddressFromVariableCRCPathInternal( const VariableCRCPath& pathCRC, unsigned int& addressResult, const unsigned int currentPathIndex, const unsigned int currentClassOffset, const TypeDefinition::VariableLinkList& variables )const
			{
				CRC crcResult = pathCRC[currentPathIndex];

				const TypeDefinition::VariableLinkListNode* currentNode = variables.HeadConst();
				while (currentNode)
				{
					const TypeVariable* currentTypeVariable = currentNode->GetPayloadConst();

					if ( !currentTypeVariable->IsPointerType() )
					{		
						bool arithmeticLeafFound = currentTypeVariable->IsArithmeticType() && crcResult == currentTypeVariable->GetNameCRC();
						bool classInPathFound = currentTypeVariable->IsClassType() && crcResult == currentTypeVariable->GetNameCRC();
						bool classLeafFound = classInPathFound && currentPathIndex+1 == pathCRC.Size();
						
						DIA_ASSERT( DIA_IMPLIES(arithmeticLeafFound, currentPathIndex+1 == pathCRC.Size()), "Can not dig deaper into a arithmetic variable %s", currentTypeVariable->GetName());

						if ( arithmeticLeafFound || classLeafFound)
						{
							const char* pointeeAsType = reinterpret_cast<const char*>(mPointeeConst);
							addressResult = reinterpret_cast<unsigned int>((((currentTypeVariable->GetOffsetFromParent() + currentClassOffset) / sizeof (char)) + pointeeAsType));

							return true;
						}
											
						if (classInPathFound)
						{
							const char* pointeeAsType = reinterpret_cast<const char*>(mPointeeConst);

							const unsigned int startClassAddress = reinterpret_cast<unsigned int>((((currentTypeVariable->GetOffsetFromParent() + currentClassOffset) / sizeof (char)) + pointeeAsType));
							const unsigned int endClassAddress = startClassAddress + currentTypeVariable->GetSize();

							unsigned int newPathIndex = currentPathIndex + 1;
							return FindPointerAddressFromVariableCRCPathInternal(pathCRC, addressResult, newPathIndex, currentClassOffset + currentTypeVariable->GetOffsetFromParent(), currentTypeVariable->GetClassDefinition()->GetVariables());
						}
					}

					currentNode = currentNode->GetNextConst();
				}

				return false;
			}
		}
	}
}