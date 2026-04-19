#ifndef DIA_TYPE_TEXT_INSTANCE_H
#define DIA_TYPE_TEXT_INSTANCE_H

#include "DiaCore/Type/TypeVariable.h"
#include "DiaCore/Type/TypeDefinition.h"

#include <cstdint>
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

				void FindVariablePathFromPointerAddress( const uintptr_t address, VariablePath& resultPath )const;
				void FindPointerAddressFromVariableCRCPath( const VariableCRCPath& pathCRC, uintptr_t& addressResult )const;

			private:
				bool FindVariablePathFromPointerAddressInternal( const uintptr_t address, VariablePath& resultPath, const uintptr_t currentClassOffset, const TypeDefinition::VariableLinkList& variables )const;
				bool FindPointerAddressFromVariableCRCPathInternal( const VariableCRCPath& pathCRC, uintptr_t& addressResult, const unsigned int currentPathIndex, const uintptr_t currentClassOffset, const TypeDefinition::VariableLinkList& variables )const;

				const TypeDefinition*	mInstanceDefinition;	// The type of object reflected in this instance
				void*					mPointee;				// A Pointer to the object reflected in this instance
				const void*				mPointeeConst;				// A Pointer to the object reflected in this instance
			};
		}
	}
}

#endif