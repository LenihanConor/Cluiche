#include "DiaCore/CRC/StringCRC.h"

#include "DiaCore/Type/BasicTypeDefines.h"
#include "DiaCore/Memory/Memory.h"

namespace Dia
{
	namespace Core
	{
		// -----------------------------------------------------------------------------
		StringCRC::StringCRC ()
			: CRC()
		{
			mString[0] = NULL;
		}

		// -----------------------------------------------------------------------------
		StringCRC::StringCRC(const char* string)
		{ 
			operator=(string);
		}

		// -----------------------------------------------------------------------------
		StringCRC::StringCRC(const StringCRC& stringCRC) 
			: CRC()
		{
			operator=(stringCRC);
		}

		// -----------------------------------------------------------------------------
		StringCRC& StringCRC::operator =(const StringCRC& stringCRC) 
		{
			CRC::operator=(stringCRC);
			CopyString(stringCRC.mString);
			return *this;
		}

		// -----------------------------------------------------------------------------
		StringCRC& StringCRC::operator =(const char* string) 
		{ 
			CRC::operator=(CRC(string)); 
			CopyString(string);
			return *this; 
		}

		// -----------------------------------------------------------------------------
		bool StringCRC::operator == (const StringCRC& crc)const
		{
			return (mCRC == crc.mCRC);
		}

		// -----------------------------------------------------------------------------
		bool StringCRC::operator != (const StringCRC& crc)const
		{
			return (mCRC != crc.mCRC);
		}

		// -----------------------------------------------------------------------------
		const char* StringCRC::AsChar () const
		{
			return &mString[0];
		}

		//------------------------------------------------
		StringCRC::operator const char* () const 
		{
			return AsChar();
		}

		// -----------------------------------------------------------------------------
		StringCRC& StringCRC::CopyString(const char* string)
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
	}
}