#include <DiaCellHistory/HistoryEncoding.h>
#include <cstring>
#include <vector>
#include <string>

namespace Dia::CellHistory
{
	namespace
	{
		// Base64 alphabet and decoding table
		static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		// Decode table: maps base64 char to value, or -1 for invalid
		static int DecodeChar(char c)
		{
			if (c >= 'A' && c <= 'Z') return c - 'A';
			if (c >= 'a' && c <= 'z') return c - 'a' + 26;
			if (c >= '0' && c <= '9') return c - '0' + 52;
			if (c == '+') return 62;
			if (c == '/') return 63;
			if (c == '=') return 0;  // Padding; value is don't-care
			return -1;  // Invalid
		}

		// Encode bytes to base64 and write to buffer.
		// Returns true if successful, false if capacity is insufficient.
		// outLen is set to number of characters written; buffer is untouched on failure.
		bool EncodeToBase64(const uint8_t* bytes, uint32_t byteCount, char* out, uint32_t capacity, uint32_t& outLen)
		{
			uint32_t needed = ((byteCount + 2) / 3) * 4;
			if (needed > capacity)
			{
				outLen = 0;
				return false;
			}

			uint32_t outIdx = 0;
			uint32_t i = 0;

			// Process full 3-byte groups
			for (; i + 3 <= byteCount; i += 3)
			{
				const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16)
					| (static_cast<uint32_t>(bytes[i + 1]) << 8)
					| bytes[i + 2];
				out[outIdx++] = kAlphabet[(v >> 18) & 63];
				out[outIdx++] = kAlphabet[(v >> 12) & 63];
				out[outIdx++] = kAlphabet[(v >> 6) & 63];
				out[outIdx++] = kAlphabet[v & 63];
			}

			// Handle remaining bytes (1 or 2)
			const uint32_t rest = byteCount - i;
			if (rest == 1)
			{
				const uint32_t v = static_cast<uint32_t>(bytes[i]) << 16;
				out[outIdx++] = kAlphabet[(v >> 18) & 63];
				out[outIdx++] = kAlphabet[(v >> 12) & 63];
				out[outIdx++] = '=';
				out[outIdx++] = '=';
			}
			else if (rest == 2)
			{
				const uint32_t v = (static_cast<uint32_t>(bytes[i]) << 16)
					| (static_cast<uint32_t>(bytes[i + 1]) << 8);
				out[outIdx++] = kAlphabet[(v >> 18) & 63];
				out[outIdx++] = kAlphabet[(v >> 12) & 63];
				out[outIdx++] = kAlphabet[(v >> 6) & 63];
				out[outIdx++] = '=';
			}

			outLen = outIdx;
			// Write NUL terminator if there's room
			if (capacity > needed)
			{
				out[outIdx] = '\0';
			}

			return true;
		}

		// Decode base64 text to bytes.
		// expectedByteCount: number of bytes expected (used to validate length).
		// Returns false on invalid base64, wrong length, or bad padding.
		bool DecodeFromBase64(const char* text, uint32_t textLen, uint8_t* out, uint32_t expectedByteCount)
		{
			// Compute expected base64 length
			uint32_t expectedB64Len = ((expectedByteCount + 2) / 3) * 4;
			if (textLen != expectedB64Len)
			{
				return false;  // Length mismatch
			}

			// Validate characters and padding
			uint32_t paddingCount = 0;
			for (uint32_t i = 0; i < textLen; ++i)
			{
				char c = text[i];
				if (c == '=')
				{
					paddingCount++;
					// Padding should only be at the end
					if (i < textLen - 2 || paddingCount > 2)
					{
						return false;
					}
				}
				else if (DecodeChar(c) < 0)
				{
					return false;  // Invalid character
				}
			}

			// Validate padding count
			if (expectedByteCount % 3 == 1)
			{
				if (paddingCount != 2) return false;
			}
			else if (expectedByteCount % 3 == 2)
			{
				if (paddingCount != 1) return false;
			}
			else  // expectedByteCount % 3 == 0
			{
				if (paddingCount != 0) return false;
			}

			// Decode
			uint32_t outIdx = 0;
			for (uint32_t i = 0; i < textLen; i += 4)
			{
				int c0 = DecodeChar(text[i]);
				int c1 = DecodeChar(text[i + 1]);
				int c2 = (i + 2 < textLen) ? DecodeChar(text[i + 2]) : 0;
				int c3 = (i + 3 < textLen) ? DecodeChar(text[i + 3]) : 0;

				if (c0 < 0 || c1 < 0 || (text[i + 2] != '=' && c2 < 0) || (text[i + 3] != '=' && c3 < 0))
				{
					return false;
				}

				uint32_t v = (static_cast<uint32_t>(c0) << 18)
					| (static_cast<uint32_t>(c1) << 12)
					| (static_cast<uint32_t>(c2) << 6)
					| static_cast<uint32_t>(c3);

				if (outIdx < expectedByteCount)
				{
					out[outIdx++] = static_cast<uint8_t>((v >> 16) & 0xFF);
				}
				if (outIdx < expectedByteCount)
				{
					out[outIdx++] = static_cast<uint8_t>((v >> 8) & 0xFF);
				}
				if (outIdx < expectedByteCount)
				{
					out[outIdx++] = static_cast<uint8_t>(v & 0xFF);
				}
			}

			return outIdx == expectedByteCount;
		}
	}

	// ========== Float32 ==========

	bool EncodeFloat32(const float* v, uint32_t n, char* out, uint32_t capacity, uint32_t& outLen)
	{
		if (n == 0)
		{
			// Empty array: capacity must be at least 1 for NUL terminator
			if (capacity > 0)
			{
				out[0] = '\0';
			}
			outLen = 0;
			return true;
		}

		// Convert float array to little-endian bytes
		uint32_t byteCount = n * sizeof(float);
		std::vector<uint8_t> bytes(byteCount);

		for (uint32_t i = 0; i < n; ++i)
		{
			uint32_t bits = 0;
			memcpy(&bits, &v[i], sizeof(uint32_t));

			// Write little-endian
			bytes[i * 4] = static_cast<uint8_t>(bits & 0xFF);
			bytes[i * 4 + 1] = static_cast<uint8_t>((bits >> 8) & 0xFF);
			bytes[i * 4 + 2] = static_cast<uint8_t>((bits >> 16) & 0xFF);
			bytes[i * 4 + 3] = static_cast<uint8_t>((bits >> 24) & 0xFF);
		}

		return EncodeToBase64(bytes.data(), byteCount, out, capacity, outLen);
	}

	bool DecodeFloat32(const char* text, uint32_t textLen, float* out, uint32_t n)
	{
		if (n == 0)
		{
			// Empty array: accept only empty string
			return textLen == 0;
		}

		uint32_t byteCount = n * sizeof(float);
		std::vector<uint8_t> bytes(byteCount);

		if (!DecodeFromBase64(text, textLen, bytes.data(), byteCount))
		{
			return false;
		}

		// Convert little-endian bytes back to float
		for (uint32_t i = 0; i < n; ++i)
		{
			uint32_t bits = static_cast<uint32_t>(bytes[i * 4])
				| (static_cast<uint32_t>(bytes[i * 4 + 1]) << 8)
				| (static_cast<uint32_t>(bytes[i * 4 + 2]) << 16)
				| (static_cast<uint32_t>(bytes[i * 4 + 3]) << 24);

			memcpy(&out[i], &bits, sizeof(float));
		}

		return true;
	}

	// ========== UInt16 ==========

	bool EncodeUInt16(const uint16_t* v, uint32_t n, char* out, uint32_t capacity, uint32_t& outLen)
	{
		if (n == 0)
		{
			// Empty array: capacity must be at least 1 for NUL terminator
			if (capacity > 0)
			{
				out[0] = '\0';
			}
			outLen = 0;
			return true;
		}

		// Convert uint16 array to little-endian bytes
		uint32_t byteCount = n * sizeof(uint16_t);
		std::vector<uint8_t> bytes(byteCount);

		for (uint32_t i = 0; i < n; ++i)
		{
			uint16_t val = v[i];
			// Write little-endian
			bytes[i * 2] = static_cast<uint8_t>(val & 0xFF);
			bytes[i * 2 + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
		}

		return EncodeToBase64(bytes.data(), byteCount, out, capacity, outLen);
	}

	bool DecodeUInt16(const char* text, uint32_t textLen, uint16_t* out, uint32_t n)
	{
		if (n == 0)
		{
			// Empty array: accept only empty string
			return textLen == 0;
		}

		uint32_t byteCount = n * sizeof(uint16_t);
		std::vector<uint8_t> bytes(byteCount);

		if (!DecodeFromBase64(text, textLen, bytes.data(), byteCount))
		{
			return false;
		}

		// Convert little-endian bytes back to uint16
		for (uint32_t i = 0; i < n; ++i)
		{
			out[i] = static_cast<uint16_t>(bytes[i * 2])
				| (static_cast<uint16_t>(bytes[i * 2 + 1]) << 8);
		}

		return true;
	}

	// ========== UInt8 ==========

	bool EncodeUInt8(const uint8_t* v, uint32_t n, char* out, uint32_t capacity, uint32_t& outLen)
	{
		if (n == 0)
		{
			// Empty array: capacity must be at least 1 for NUL terminator
			if (capacity > 0)
			{
				out[0] = '\0';
			}
			outLen = 0;
			return true;
		}

		return EncodeToBase64(v, n, out, capacity, outLen);
	}

	bool DecodeUInt8(const char* text, uint32_t textLen, uint8_t* out, uint32_t n)
	{
		if (n == 0)
		{
			// Empty array: accept only empty string
			return textLen == 0;
		}

		return DecodeFromBase64(text, textLen, out, n);
	}

	// ========== Helper ==========

	uint32_t EncodedLength(uint32_t byteCount)
	{
		return ((byteCount + 2) / 3) * 4;
	}
}
