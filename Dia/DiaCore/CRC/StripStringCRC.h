#ifndef DIA_STRIP_STRING_CRC_H
#define DIA_STRIP_STRING_CRC_H

#include "DiaCore/crc/CRC.h"

namespace Dia
{
	namespace Core
	{
		class StripStringCRC : public CRC
		{
		public:
			static const int kStringLength = 64;

			StripStringCRC ();
			StripStringCRC (const StripStringCRC& crc);
			StripStringCRC (const char* string); 

			StripStringCRC&		operator = (const StripStringCRC& crc);
			StripStringCRC&		operator = (const char* val);
			
			bool				operator == (const StripStringCRC& crc)const;
			bool				operator != (const StripStringCRC& crc)const;

#ifdef DEBUG
			const char*			AsChar () const;			
			operator const	char* () const;
		protected:

			StripStringCRC&		CopyString(const char* string);
			char				mString [kStringLength];
#endif
		}; // CRC
	}
}

#endif // DIA_ASSERT