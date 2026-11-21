#include "DiaCore/CRC/StringCRC.h"

#include "DiaCore/Core/BasicTypeDefines.h"
#include "DiaCore/Memory/Memory.h"

namespace Dia
{
	namespace Core
	{
		// -----------------------------------------------------------------------------
		const StringCRC StringCRC::kZero;

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
				// Copy only up to the source terminator: the source may be a short literal.
				unsigned int length = 0;
				while (length < sizeof(mString) - 1 && string[length] != '\0')
				{
					++length;
				}
				if (length > 0)
				{
					Dia::Core::MemoryCopy(mString, string, length);
				}
				for (unsigned int i = length; i < sizeof(mString); ++i)
				{
					mString[i] = '\0';
				}
			}
			else
			{
				mString[0] = '\0';
			}
			return *this;
		}
	}
}