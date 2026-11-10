#include <DiaValueSchema/RampData.h>

namespace Dia::ValueSchema
{
	namespace
	{
		int HexDigit(char c)
		{
			if (c >= '0' && c <= '9') { return c - '0'; }
			if (c >= 'a' && c <= 'f') { return 10 + (c - 'a'); }
			if (c >= 'A' && c <= 'F') { return 10 + (c - 'A'); }
			return -1;
		}
	}

	bool ParseHexColour(const char* text, uint32_t& rgba8)
	{
		if (text == nullptr || text[0] != '#') { return false; }
		uint32_t v = 0;
		for (int i = 1; i <= 6; ++i)
		{
			const int d = HexDigit(text[i]);	// stops at the terminator for short input (not hex)
			if (d < 0) { return false; }
			v = (v << 4) | static_cast<uint32_t>(d);
		}
		if (text[7] != '\0') { return false; }
		rgba8 = (v << 8) | 0xFFu;
		return true;
	}
}
