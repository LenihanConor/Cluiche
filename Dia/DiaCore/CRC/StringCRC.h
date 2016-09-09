#ifndef DIA_STRING_CRC_H
#define DIA_STRING_CRC_H

#include "DiaCore/crc/CRC.h"

namespace Dia
{
	namespace Core
	{
		class StringCRC : public CRC
		{
		public:
			static const StringCRC kZero;

			static const int kStringLength = 64;
			static const int Capacity() { return kStringLength; }

			StringCRC ();
			StringCRC (const StringCRC& crc);
			StringCRC (const char* string); 

			StringCRC&			operator = (const StringCRC& crc);
			StringCRC&			operator = (const char* val);
			
			bool				operator == (const StringCRC& crc)const;
			bool				operator != (const StringCRC& crc)const;

			const char*			AsChar () const;
			
			operator const	char* () const;
		protected:
			StringCRC&			CopyString(const char* string);

			char				mString [kStringLength];
		}; // CRC

	}
}

#endif // DIA_ASSERT