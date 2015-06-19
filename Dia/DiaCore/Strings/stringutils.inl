
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <DiaCore/Core/Assert.h>

#pragma warning(disable : 4996)

namespace Dia
{
	namespace Core
	{
		inline
		char* StringCopy (char* dst, const char* src, const int n)
		{ 
			DIA_ASSERT( dst + n <= src || src + n <= dst, "Src to small" );
			return strncpy(dst, src, n); 
		}

		inline
		char* StringCopyOverlapping (char* dst, const char* src, const int n)
		{ 
			return strncpy(dst, src, n); 
		}	

		inline
		char* StringConcat (char* dst, const char* src, const int n)
		{
			return strncat(dst, src, n);
		}

		inline 
		unsigned int StringLength (const char* str)
		{ 
			return static_cast<unsigned int>(strlen(str)); 
		}

		inline 
		int	StringFormat(char* buffer, const unsigned int n, const char* format, ...)
		{
			va_list va;
			va_start(va, format);
			int result = StringFormat(buffer, n, format, va);
			va_end(va);
			return result;
		}

		inline
		int	StringFormat(char* buffer, const unsigned int n, const char* format, va_list args)
		{ 
			return vsnprintf(buffer, n, format, args); 
		}

		inline 
		char* StringFindChar (const char* buffer, char ch)
		{ 
			return strchr(const_cast<char*>(buffer), ch); 
		}

		inline 
		char* StringFindChar (const char* buffer, const char* chars)
		{ 
			return strpbrk(const_cast<char*>(buffer), chars);
		}

		inline 
		char* StringRevFindChar (const char* buffer, char ch)
		{ 
			return strrchr(const_cast<char*>(buffer), ch); 
		}

		inline 
		const char* StringFindString (const char* buffer, const char* str)
		{
			return strstr(buffer, str);
		}

		inline 
		int StringCompare (const char* str1, const char* str2, const int num)
		{ 
			return strncmp(str1, str2, num); 
		}

		inline 
		int StringCompareNoCase (const char* str1, const char* str2, const int num)
		{

			return _strnicmp(str1, str2, num); 
		}
		
		inline
		void StringToWString(const char* input, wchar_t* output)
		{
			swprintf(output, L"%S", input);
		}

		inline 
		void StringConvertFromShort	(char* result, const int resultCapacity, const short val)
		{
			StringFormat( result, resultCapacity, "%hi", val );
		}
		
		inline 
		void StringConvertFromUShort(char* result, const int resultCapacity, const unsigned short val)
		{
			StringFormat( result, resultCapacity, "%hu", val );
		}
		
		inline 
		void StringConvertFromInt (char* result, const int val, int base)
		{
			itoa(val, result, base); 
		}

		inline 
		void StringConvertFromUInt (char* result, const unsigned int val, int base)
		{
			itoa(val, result, base); 
		}

		inline 
		void StringConvertFromInt64 (char* result, const unsigned long long val, int base)
		{
			_i64toa(val, result, base);
		}

		inline 
		void StringConvertFromUInt64 (char* result, const unsigned long long val, int base)
		{
			_ui64toa(val, result, base);
		}

		inline 
		void StringConvertFromFloat (char* result, const int resultCapacity, const float val)
		{
			StringFormat( result, resultCapacity, "%f", val );
		}

		inline 
		void StringConvertFromDouble (char* result, const int resultCapacity, const double val)
		{
			StringFormat( result, resultCapacity, "%f", val );
		}

		inline
		short StringConvertToShort(const char* str)
		{
			return static_cast<short>(atoi(str));
		}

		inline
		unsigned int StringConvertToUShort(const char* str, unsigned int base)
		{
			return static_cast<unsigned short>(strtoul(str, NULL, base));
		}

		inline
		int	StringConvertToInt (const char* str)
		{
			return (atoi(str));
		}

		inline
		unsigned int StringConvertToUInt (const char* str, unsigned int base)
		{
			return static_cast<unsigned int>(strtoul(str, NULL, base));
		}

		inline
		long	StringConvertToLong (const char* str)
		{
			return atol(str);
		}

		inline
		unsigned long StringConvertToULong (const char* str, unsigned int base)
		{
			return strtoul(str, NULL, base);
		}

		inline
		Dia::Core::Int64	StringConvertToInt64	(const char* str, unsigned int base )
		{
			return _strtoi64(str, NULL, base);		
		}

		inline
		Dia::Core::UInt64	StringConvertToUInt64	(const char* str, unsigned int base )
		{
			return _strtoui64(str, NULL, base);		
		}

		inline
		float StringConvertToFloat (const char* str)
		{
			return static_cast<float>(strtod(str, NULL));
		}

		inline
		double	StringConvertToDouble	(const char* str)
		{
			return strtod(str, NULL);
		}

		inline
		void StringToLower (char* str)
		{
			for (; *str; ++str)
			{
				if (*str >= 'A' && *str <= 'Z')
				{
					*str += 32;
				}
			}
		}
	}
}
