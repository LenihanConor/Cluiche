#include "DiaCore/Memory/Memory.h"

#include "DiaCore/Core/Assert.h"

#include <string.h>

namespace Dia
{
	namespace Core
	{
		void MemoryCopy (void* dst, const void* src, unsigned int size)
		{
			DIA_ASSERT(dst != NULL, "dst NULL");
			DIA_ASSERT(src != NULL, "src NULL");
			DIA_ASSERT(size != 0, "size != 0");
			
			memcpy(dst,src,size);
		}

		void MemorySet (void* dst, int value, unsigned int size)
		{
			DIA_ASSERT(dst != NULL, "dst NULL");
			DIA_ASSERT(size != 0, "size != 0");

			memset(dst,value,size);
		}

		int MemoryCompare (const void* src1, const void* src2, unsigned int size)
		{
			DIA_ASSERT(src1 != NULL, "dst NULL");
			DIA_ASSERT(src2 != NULL, "src NULL");
			DIA_ASSERT(size != 0, "size != 0");

			return memcmp(src1,src2,size);
		}
	}
}