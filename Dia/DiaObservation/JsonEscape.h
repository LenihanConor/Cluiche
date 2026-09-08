#pragma once

#include <cstddef>

namespace Dia
{
	namespace Observation
	{
		inline void EscapeJsonString(const char* src, char* dst, size_t dstSize)
		{
			if (!src || dstSize == 0)
			{
				if (dstSize > 0) dst[0] = '\0';
				return;
			}

			size_t di = 0;
			for (size_t si = 0; src[si] != '\0' && di < dstSize - 1; ++si)
			{
				char c = src[si];
				if (c == '"' || c == '\\')
				{
					if (di + 2 >= dstSize) break;
					dst[di++] = '\\';
					dst[di++] = c;
				}
				else if (c == '\n')
				{
					if (di + 2 >= dstSize) break;
					dst[di++] = '\\';
					dst[di++] = 'n';
				}
				else if (c == '\r')
				{
					if (di + 2 >= dstSize) break;
					dst[di++] = '\\';
					dst[di++] = 'r';
				}
				else if (c == '\t')
				{
					if (di + 2 >= dstSize) break;
					dst[di++] = '\\';
					dst[di++] = 't';
				}
				else
				{
					dst[di++] = c;
				}
			}
			dst[di] = '\0';
		}
	} // namespace Observation
} // namespace Dia
