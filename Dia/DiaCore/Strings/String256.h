#ifndef STRING256
#define STRING256

#include "DiaCore/Strings/String.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String256 : public String<256>
			{
			public:
				String256();																			
				explicit String256 ( ConstIterator& iter ); 
				explicit String256 ( ConstReverseIterator& iter ); 
				template<class Evaluator> explicit String256 ( ConstIterator& iter, const Evaluator& filter);

				// Single-argument form is NON-formatting (a '%' in paths/keys is literal); pass format args to use the printf-style overload below.
				String256(const char* pRawString) : String<256>() { SetText(pRawString); }
				template<class... Args> requires (sizeof...(Args) > 0) String256(const char* pRawString, Args... args) : String<256>() { DIA_ASSERT(pRawString != NULL, "Invalid Data"); FormatArgs(pRawString, args...); }
				String256(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String256.inl"

#endif