#ifndef STRING64
#define STRING64

#include "DiaCore/Strings/String.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String64 : public String<64>
			{
			public:
				String64();													
				explicit String64 ( ConstIterator& iter ); 
				explicit String64 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String64 (const String<_size>& rhs);
				template<class Evaluator> explicit String64 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String64 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				// Single-argument form is NON-formatting (a '%' in paths/keys is literal); pass format args to use the printf-style overload below.
				String64(const char* pRawString) : String<64>() { SetText(pRawString); }
				template<class... Args> requires (sizeof...(Args) > 0) String64(const char* pRawString, Args... args) : String<64>() { DIA_ASSERT(pRawString != NULL, "Invalid Data"); FormatArgs(pRawString, args...); }
				String64(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String64.inl"

#endif