#include "DiaCore/CRC/StripStringCRC.h"

#include "DiaCore/Type/BasicTypeDefines.h"
#include "DiaCore/Memory/Memory.h"

namespace Dia
{
	namespace Core
	{
		// -----------------------------------------------------------------------------
		StripStringCRC::StripStringCRC ()
			: CRC()
		{
#ifdef DEBUG		
			mString[0] = NULL;
#endif
		}

		// -----------------------------------------------------------------------------
		StripStringCRC::StripStringCRC(const char* string)
		{ 
			operator=(string);
		}

		// -----------------------------------------------------------------------------
		StripStringCRC::StripStringCRC(const StripStringCRC& stringCRC) 
			: CRC()
		{
			operator=(stringCRC);
		}

		// -----------------------------------------------------------------------------
		StripStringCRC& StripStringCRC::operator =(const StripStringCRC& stringCRC) 
		{
			CRC::operator=(stringCRC);
#ifdef DEBUG
			CopyString(stringCRC.mString);
#endif
			return *this;
		}

		// -----------------------------------------------------------------------------
		StripStringCRC& StripStringCRC::operator =(const char* string) 
		{ 
			CRC::operator=(CRC(string)); 
#ifdef DEBUG
			CopyString(string);
#endif
			return *this; 
		}

		// -----------------------------------------------------------------------------
		bool StripStringCRC::operator == (const StripStringCRC& crc)const
		{
			return (mCRC == crc.mCRC);
		}

		// -----------------------------------------------------------------------------
		bool StripStringCRC::operator != (const StripStringCRC& crc)const
		{
			return (mCRC != crc.mCRC);
		}

#ifdef DEBUG
		// -----------------------------------------------------------------------------
		const char* StripStringCRC::AsChar () const
		{
			return &mString[0];
		}

		//------------------------------------------------
		StripStringCRC::operator const char* () const 
		{
			return AsChar();
		}

		// -----------------------------------------------------------------------------
		StripStringCRC& StripStringCRC::CopyString(const char* string)
		{
			if(string!=NULL)
			{
				Dia::Core::MemoryCopy(mString, string, sizeof(mString));
				mString[sizeof(mString) - 1] = '\0';
			}
			else
			{
				mString[0] = '\0';
			}
			return *this;
		}
#endif
	}
}