#pragma once

// Base64 (RFC 4648, standard alphabet, '=' padding). Header-only, no allocation beyond the returned string.

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace Dia
{
	namespace Core
	{
		namespace Base64
		{
			inline size_t EncodedSize(size_t byteCount) { return ((byteCount + 2) / 3) * 4; }

			// Appends the base64 text of bytes[0..byteCount) to `out`.
			inline void Encode(const uint8_t* bytes, size_t byteCount, std::string& out)
			{
				static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				out.reserve(out.size() + EncodedSize(byteCount));
				size_t i = 0;
				for (; i + 3 <= byteCount; i += 3)
				{
					const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8) | bytes[i + 2];
					out.push_back(kAlphabet[(v >> 18) & 63]);
					out.push_back(kAlphabet[(v >> 12) & 63]);
					out.push_back(kAlphabet[(v >> 6) & 63]);
					out.push_back(kAlphabet[v & 63]);
				}
				const size_t rest = byteCount - i;
				if (rest == 1)
				{
					const uint32_t v = static_cast<uint32_t>(bytes[i]) << 16;
					out.push_back(kAlphabet[(v >> 18) & 63]);
					out.push_back(kAlphabet[(v >> 12) & 63]);
					out.push_back('=');
					out.push_back('=');
				}
				else if (rest == 2)
				{
					const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8);
					out.push_back(kAlphabet[(v >> 18) & 63]);
					out.push_back(kAlphabet[(v >> 12) & 63]);
					out.push_back(kAlphabet[(v >> 6) & 63]);
					out.push_back('=');
				}
			}

			inline std::string Encode(const uint8_t* bytes, size_t byteCount)
			{
				std::string s;
				Encode(bytes, byteCount, s);
				return s;
			}
		}
	}
}
