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

				String512(const char* pRawString, ...);
				String512(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String512.inl"

#endif