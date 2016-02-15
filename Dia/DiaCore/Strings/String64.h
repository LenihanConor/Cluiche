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
				DIA_TYPE_DECLARATION;

				String64();													
				explicit String64 ( ConstIterator& iter ); 
				explicit String64 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String64 (const String<_size>& rhs);
				template<class Evaluator> explicit String64 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String64 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				String64(const char* pRawString, ...);
				String64(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String64.inl"

#endif