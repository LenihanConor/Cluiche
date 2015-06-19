#ifndef DIA_STRING_UTILITIES__
#define DIA_STRING_UTILITIES__

#include <stdarg.h>

namespace Dia
{
	namespace Core
	{
		char*				StringCopy				(char* dst, const char* src, const int n);
		char*				StringCopyOverlapping	(char* dst, const char* src, const int n);
		char*				StringConcat			(char* dst, const char* src, const int n);
		unsigned int		StringLength			(const char* str);
		int					StringFormat			(char* buffer, const unsigned int n, const char* format, ...);
		int					StringFormat			(char* buffer, const unsigned int n, const char* format, va_list args);
		char*				StringFindChar			(const char* buffer, char ch);
		char*				StringRevFindChar		(const char* buffer, char ch);
		const char*			StringFindString		(const char* buffer, const char* str);
		int					StringCompare			(const char* str1, const char* str2, const int n);
		int					StringCompareNoCase		(const char* str1, const char* str2, const int n);
		void				StringToLower			(char* str);
		void				StringToUpper			(char* str);
		void				StringToWString			(const char* input, wchar_t* output);

		// Create String from Primitive
		void				StringConvertFromShort	(char* result, const int resultCapacity, const short val);
		void				StringConvertFromUShort	(char* result, const int resultCapacity, const unsigned short val);
		void				StringConvertFromInt	(char* result, const int val, int base = 10);
		void				StringConvertFromUInt	(char* result, const unsigned int val, int base = 10);
		void				StringConvertFromInt64	(char* result, const unsigned long long val, int base = 10);
		void				StringConvertFromUInt64	(char* result, const unsigned long long val, int base = 10);
		void				StringConvertFromFloat	(char* result, const int resultCapacity, const float val);
		void				StringConvertFromDouble	(char* result, const int resultCapacity, const double val);
		
		// Create primitives from string
		short				StringConvertToShort	(const char* str); 
		unsigned int		StringConvertToUShort	(const char* str, unsigned int base = 0);
		int					StringConvertToInt		(const char* str); 
		unsigned int		StringConvertToUInt		(const char* str, unsigned int base = 0);
		long				StringConvertToLong		(const char* str); 
		unsigned long		StringConvertToULong	(const char* str, unsigned int base = 0);
		Int64				StringConvertToInt64	(const char* str, unsigned int base = 0);
		UInt64				StringConvertToUInt64	(const char* str, unsigned int base = 0);
		float				StringConvertToFloat	(const char* str);	
		double				StringConvertToDouble	(const char* str);	
	}
} 

#include "DiaCore/Strings/stringutils.inl"

#endif 

