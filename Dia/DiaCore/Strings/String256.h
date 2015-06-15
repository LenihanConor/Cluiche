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

				String256(const char* pRawString, ...);
				String256(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String256.inl"

#endif