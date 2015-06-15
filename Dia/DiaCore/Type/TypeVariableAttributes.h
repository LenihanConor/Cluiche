#ifndef DIA_TYPE_VARIABLE_ATTRIBUTES_H
#define DIA_TYPE_VARIABLE_ATTRIBUTES_H

#include "DiaCore/CRC/StripStringCRC.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeVariable;

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributes
			//---------------------------------------------------------------------------------------------------------
			class TypeVariableAttributes
			{
			public:

				TypeVariableAttributes(){};
				virtual ~TypeVariableAttributes(){};
				
				virtual const StripStringCRC& GetUniqueID()const = 0;
				virtual void AssignedTo(const TypeVariable& typeVariable) = 0;
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesPointerAsObject
			//---------------------------------------------------------------------------------------------------------
			// Changes the serialization behavior of a pointer to act like an object
			class TypeVariableAttributesPointerAsObject : public TypeVariableAttributes
			{
			public:
				TypeVariableAttributesPointerAsObject();	 
				
				static const StripStringCRC mAttributeID;
				static const StripStringCRC& GetStaticUniqueID(){return mAttributeID; };
				virtual const StripStringCRC& GetUniqueID()const{ return mAttributeID;};
				virtual void AssignedTo(const TypeVariable& typeVariable);
			};
		}
	}
}

#endif // DIA_ASSERT