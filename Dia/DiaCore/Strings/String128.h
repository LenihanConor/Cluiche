#ifndef STRING128
#define STRING128

#include "DiaCore/Strings/String.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String128 : public String<128>
			{
			public:
				String128();																				
				explicit String128 ( ConstIterator& iter ); 
				explicit String128 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String128 (const String<_size>& rhs);
				template<class Evaluator> explicit String128 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String128 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				// Single-argument form is NON-formatting (a '%' in paths/keys is literal); pass format args to use the printf-style overload below.
				String128(const char* pRawString) : String<128>() { SetText(pRawString); }
				template<class... Args> requires (sizeof...(Args) > 0) String128(const char* pRawString, Args... args) : String<128>() { DIA_ASSERT(pRawString != NULL, "Invalid Data"); FormatArgs(pRawString, args...); }
				String128(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String128.inl"

#endif