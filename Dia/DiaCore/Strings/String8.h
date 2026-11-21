#ifndef STRING8
#define STRING8

#include "DiaCore/Strings/String.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String8 : public String<8>
			{
			public:
				String8();													
				explicit String8 ( ConstIterator& iter ); 
				explicit String8 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String8 (const String<_size>& rhs);
				template<class Evaluator> explicit String8 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String8 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				// Single-argument form is NON-formatting (a '%' in paths/keys is literal); pass format args to use the printf-style overload below.
				String8(const char* pRawString) : String<8>() { SetText(pRawString); }
				template<class... Args> requires (sizeof...(Args) > 0) String8(const char* pRawString, Args... args) : String<8>() { DIA_ASSERT(pRawString != NULL, "Invalid Data"); FormatArgs(pRawString, args...); }
				String8(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String8.inl"

#endif