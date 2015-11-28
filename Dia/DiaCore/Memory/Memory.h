#ifndef MEMORY_H
#define MEMORY_H

#	define DIA_NEW(obj)					new  obj
#	define DIA_NEW_ARRAY(size, obj)		new obj[size]
#	define DIA_DELETE(mem)				delete (mem); mem = nullptr
#	define DIA_DELETE_ARRAY(mem)			delete[] (mem); mem = nullptr

namespace Dia
{
	namespace Core
	{
		void		MemoryCopy		(void* dst, const void* src, unsigned int size);
		void		MemorySet		(void* dst, int value, unsigned int size);
		int			MemoryCompare	(const void* src1, const void* src2, unsigned int size);
	}
}
#endif	// _DATADEFS_H