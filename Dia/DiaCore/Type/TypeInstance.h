#ifndef DIA_TYPE_TEXT_INSTANCE_H
#define DIA_TYPE_TEXT_INSTANCE_H

#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeDefinition.h"

#include <vector>

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeDefinition;
			//------------------------------------------------------------------------------------
			//	TypeInstance
			//------------------------------------------------------------------------------------
			class TypeInstance
			{
			public:
				typedef std::vector<const TypeVariable*> VariablePath;
				typedef std::vector<CRC> VariableCRCPath;

				TypeInstance();
				TypeInstance(const TypeDefinition* typeDefinition, void* pointee);
				TypeInstance(const TypeDefinition* typeDefinition, const void* pointee);
				
				const TypeDefinition*	GetTypeDescriptor		() const;
				void*					Pointee					();
				const void*				Pointee					() const;

				unsigned int NumFields()const;
				const TypeDefinition::VariableLinkList& GetVariables()const;

				void FindVariablePathFromPointerAddress( const unsigned int address, VariablePath& resultPath )const;
				void FindPointerAddressFromVariableCRCPath( const VariableCRCPath& pathCRC, unsigned int& addressResult )const;

			private:
				bool FindVariablePathFromPointerAddressInternal( const unsigned int address,VariablePath& resultPath, const unsigned int currentClassOffset, const TypeDefinition::VariableLinkList& variables )const;
				bool FindPointerAddressFromVariableCRCPathInternal( const VariableCRCPath& pathCRC, unsigned int& addressResult, const unsigned int currentPathIndex, const unsigned int currentClassOffset, const TypeDefinition::VariableLinkList& variables )const;

				const TypeDefinition*	mInstanceDefinition;	// The type of object reflected in this instance
				void*					mPointee;				// A Pointer to the object reflected in this instance
				const void*				mPointeeConst;				// A Pointer to the object reflected in this instance
			};
		}
	}
}

#endif