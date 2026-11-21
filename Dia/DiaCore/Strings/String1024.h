#ifndef STRING1024
#define STRING1024

#include "DiaCore/Strings/String.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String1024 : public String<1024>
			{
			public:
				String1024();																				
				explicit String1024 ( ConstIterator& iter ); 
				explicit String1024 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String1024 (const String<_size>& rhs);
				template<class Evaluator> explicit String1024 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String1024 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				// Single-argument form is NON-formatting (a '%' in paths/keys is literal); pass format args to use the printf-style overload below.
				String1024(const char* pRawString) : String<1024>() { SetText(pRawString); }
				template<class... Args> requires (sizeof...(Args) > 0) String1024(const char* pRawString, Args... args) : String<1024>() { DIA_ASSERT(pRawString != NULL, "Invalid Data"); FormatArgs(pRawString, args...); }
				String1024(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String1024.inl"

#endif