#pragma once

#include <cstddef>

namespace Dia::CellHistory
{
	// Largest length <= maxBytes (and <= strlen(s)) that does not end inside a UTF-8 multi-byte sequence.
	// A trailing lead byte whose continuation bytes are missing is dropped. Stray or invalid bytes are left alone.
	inline size_t Utf8SafeLength(const char* s, size_t maxBytes)
	{
		if (s == nullptr) { return 0; }
		size_t n = 0;
		while (n < maxBytes && s[n] != '\0') { ++n; }

		size_t j = n;
		size_t continuation = 0;
		while (j > 0 && continuation < 3 && (static_cast<unsigned char>(s[j - 1]) & 0xC0u) == 0x80u)
		{
			--j;
			++continuation;
		}
		if (j == 0) { return n; }

		const unsigned char lead = static_cast<unsigned char>(s[j - 1]);
		size_t need = 1;
		if (lead >= 0xF0u && lead <= 0xF7u) { need = 4; }
		else if (lead >= 0xE0u && lead <= 0xEFu) { need = 3; }
		else if (lead >= 0xC0u && lead <= 0xDFu) { need = 2; }

		if (need > 1 && continuation < need - 1) { return j - 1; }
		return n;
	}
}
