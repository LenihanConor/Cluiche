#ifndef STRING32
#define STRING32

#include "DiaCore/Strings/String.h"
#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String32 : public String<32>
			{
			public:
				DIA_TYPE_DECLARATION;

				String32();													
				explicit String32 ( ConstIterator& iter ); 
				explicit String32 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String32 (const String<_size>& rhs);
				template<class Evaluator> explicit String32 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String32 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				String32(const char* pRawString, ...);
				String32(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String32.inl"

#endif