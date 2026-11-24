#ifndef STRING512
#define STRING512

#include "DiaCore/Strings/String.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String512 : public String<512>
			{
			public:
				String512();													
				explicit String512 ( ConstIterator& iter ); 
				explicit String512 ( ConstReverseIterator& iter ); 
				template<class Evaluator> explicit String512 ( ConstIterator& iter, const Evaluator& filter);

				// Single-argument form is NON-formatting (a '%' in paths/keys is literal); pass format args to use the printf-style overload below.
				String512(const char* pRawString) : String<512>() { SetText(pRawString); }
				template<class... Args> requires (sizeof...(Args) > 0) String512(const char* pRawString, Args... args) : String<512>() { DIA_ASSERT(pRawString != NULL, "Invalid Data"); FormatArgs(pRawString, args...); }
				String512(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String512.inl"

#endif