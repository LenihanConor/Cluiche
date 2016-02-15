#ifndef STRING8
#define STRING8

#include "DiaCore/Strings/String.h"

#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			class String8 : public String<8>
			{
			public:
				DIA_TYPE_DECLARATION;

				String8();													
				explicit String8 ( ConstIterator& iter ); 
				explicit String8 ( ConstReverseIterator& iter ); 
				template<unsigned int _size> explicit String8 (const String<_size>& rhs);
				template<class Evaluator> explicit String8 ( ConstIterator& iter, const Evaluator& filter);
				template<unsigned int _size> explicit String8 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements ); 

				String8(const char* pRawString, ...);
				String8(const char* pRawString, va_list argList);
			};
		}
	}
}

#include "DiaCore/Strings/String8.inl"

#endif