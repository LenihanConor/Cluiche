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
				DIA_TYPE_DECLARATION;

				String128();																				
				explicit String128 ( ConstIterator& iter ); 
				explicit String128 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String128 (const String<_size>& rhs);
				template<class Evaluator> explicit String128 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String128 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				String128(const char* pRawString, ...);
				String128(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String128.inl"

#endif