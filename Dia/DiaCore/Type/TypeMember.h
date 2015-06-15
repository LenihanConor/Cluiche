#ifndef DIA_TYPE_MEMBER_H
#define DIA_TYPE_MEMBER_H

#include "DiaCore/crc/StringCRC.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeMember
			{
			public:
				TypeMember();
				TypeMember(const char* name);

				const char*					GetName() const				{ return mNameID.AsChar(); }
				const StringCRC&			GetStringCRC() const		{ return mNameID; }
				const CRC&					GetNameCRC() const			{ return mNameID; }
				const unsigned int			GetHashID() const			{ return mNameID.Value(); }

			private:
				StringCRC mNameID;		 
			};
		}
	}
}

#endif // DIA_ASSERT