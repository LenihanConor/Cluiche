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
				DIA_TYPE_DECLARATION;

				String1024();																				
				explicit String1024 ( ConstIterator& iter ); 
				explicit String1024 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String1024 (const String<_size>& rhs);
				template<class Evaluator> explicit String1024 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String1024 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				String1024(const char* pRawString, ...);
				String1024(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String1024.inl"

#endif