#include <gtest/gtest.h>
#include <DiaCellHistory/HistoryEncoding.h>
#include <cstring>
#include <vector>
#include <string>

using namespace Dia::CellHistory;

// ======== Float32 Tests ========

TEST(HistoryEncoding, EncodeFloat32Empty)
{
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(nullptr, 0, buffer, sizeof(buffer), outLen));
	EXPECT_EQ(outLen, 0);
	EXPECT_EQ(buffer[0], '\0');  // NUL should be written
}

TEST(HistoryEncoding, EncodeFloat32OneElement)
{
	float v = 1.0f;
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(&v, 1, buffer, sizeof(buffer), outLen));
	EXPECT_EQ(outLen, 8);  // 4 bytes -> 8 base64 chars with padding
	// 1.0f in little-endian float32 is: 00 00 80 3F
	// Base64: "AACAPw=="
	EXPECT_EQ(std::string(buffer, outLen), "AACAPw==");
	EXPECT_EQ(buffer[outLen], '\0');  // NUL should be written after
}

TEST(HistoryEncoding, EncodeFloat32TwoElements)
{
	float v[2] = { 1.0f, -1.0f };
	char buffer[32];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(v, 2, buffer, sizeof(buffer), outLen));
	EXPECT_EQ(outLen, 12);  // 8 bytes -> base64 length 12
	EXPECT_EQ(buffer[outLen], '\0');  // NUL should be written after
	// Verify it decodes back
	std::vector<float> decoded(2);
	EXPECT_TRUE(DecodeFloat32(buffer, outLen, decoded.data(), 2));
	EXPECT_EQ(decoded[0], 1.0f);
	EXPECT_EQ(decoded[1], -1.0f);
}

TEST(HistoryEncoding, EncodeFloat32CapacityExactly)
{
	float v = 1.0f;
	char buffer[8];  // Exactly needed for "AACAPw=="
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(&v, 1, buffer, 8, outLen));
	EXPECT_EQ(outLen, 8);
	EXPECT_EQ(std::string(buffer, outLen), "AACAPw==");
	// No NUL space available
}

TEST(HistoryEncoding, EncodeFloat32CapacityTooSmall)
{
	float v = 1.0f;
	char buffer[8];
	uint32_t outLen = 99;  // Initialize to non-zero
	memset(buffer, 0xCC, sizeof(buffer));  // Trash the buffer
	// Needed is 8, capacity is 7
	EXPECT_FALSE(EncodeFloat32(&v, 1, buffer, 7, outLen));
	EXPECT_EQ(outLen, 0);
	// Buffer should be untouched (still trash)
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_EQ(static_cast<unsigned char>(buffer[i]), 0xCC);
	}
}

TEST(HistoryEncoding, EncodeFloat32NaN)
{
	float v = std::nanf("");
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(&v, 1, buffer, sizeof(buffer), outLen));
	float decoded;
	EXPECT_TRUE(DecodeFloat32(buffer, outLen, &decoded, 1));
	// The exact NaN payload should be preserved
	uint32_t originalBits, decodedBits;
	memcpy(&originalBits, &v, sizeof(uint32_t));
	memcpy(&decodedBits, &decoded, sizeof(uint32_t));
	EXPECT_EQ(decodedBits, originalBits);
}

TEST(HistoryEncoding, EncodeFloat32Infinity)
{
	float v[2] = { INFINITY, -INFINITY };
	char buffer[32];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(v, 2, buffer, sizeof(buffer), outLen));
	std::vector<float> decoded(2);
	EXPECT_TRUE(DecodeFloat32(buffer, outLen, decoded.data(), 2));
	EXPECT_TRUE(std::isinf(decoded[0]) && decoded[0] > 0);
	EXPECT_TRUE(std::isinf(decoded[1]) && decoded[1] < 0);
}

TEST(HistoryEncoding, EncodeFloat32NegativeZero)
{
	float v = -0.0f;
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(&v, 1, buffer, sizeof(buffer), outLen));
	float decoded;
	EXPECT_TRUE(DecodeFloat32(buffer, outLen, &decoded, 1));
	uint32_t originalBits, decodedBits;
	memcpy(&originalBits, &v, sizeof(uint32_t));
	memcpy(&decodedBits, &decoded, sizeof(uint32_t));
	EXPECT_EQ(decodedBits, originalBits);  // -0.0 has sign bit set
}

TEST(HistoryEncoding, EncodeFloat32Denormal)
{
	// Smallest positive denormal
	float v = std::numeric_limits<float>::denorm_min();
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(&v, 1, buffer, sizeof(buffer), outLen));
	float decoded;
	EXPECT_TRUE(DecodeFloat32(buffer, outLen, &decoded, 1));
	EXPECT_EQ(decoded, v);
}

TEST(HistoryEncoding, EncodeFloat32Max)
{
	float v = std::numeric_limits<float>::max();
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(&v, 1, buffer, sizeof(buffer), outLen));
	float decoded;
	EXPECT_TRUE(DecodeFloat32(buffer, outLen, &decoded, 1));
	EXPECT_EQ(decoded, v);
}

TEST(HistoryEncoding, EncodeFloat32RoundTripVariousLengths)
{
	std::vector<float> test = { 1.5f, 2.5f, 3.5f, 4.5f, 5.5f };
	for (size_t n = 0; n <= test.size(); ++n)
	{
		uint32_t needed = EncodedLength(n * sizeof(float)) + 1;
		std::vector<char> buffer(needed);
		uint32_t outLen = 0;
		EXPECT_TRUE(EncodeFloat32(test.data(), n, buffer.data(), buffer.size(), outLen)) << "Failed for n=" << n;
		std::vector<float> decoded(n);
		EXPECT_TRUE(DecodeFloat32(buffer.data(), outLen, decoded.data(), n)) << "Failed for n=" << n;
		for (size_t i = 0; i < n; ++i)
		{
			EXPECT_EQ(decoded[i], test[i]) << "Mismatch at index " << i << " for n=" << n;
		}
	}
}

TEST(HistoryEncoding, EncodeFloat32LargeArray)
{
	std::vector<float> v(100000);
	for (size_t i = 0; i < v.size(); ++i)
	{
		v[i] = static_cast<float>(i) * 0.1f;
	}
	uint32_t needed = EncodedLength(v.size() * sizeof(float)) + 1;
	std::vector<char> buffer(needed);
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeFloat32(v.data(), v.size(), buffer.data(), buffer.size(), outLen));
	std::vector<float> decoded(v.size());
	EXPECT_TRUE(DecodeFloat32(buffer.data(), outLen, decoded.data(), v.size()));
	for (size_t i = 0; i < v.size(); ++i)
	{
		EXPECT_EQ(decoded[i], v[i]);
	}
}

// ======== UInt16 Tests ========

TEST(HistoryEncoding, EncodeUInt16Empty)
{
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt16(nullptr, 0, buffer, sizeof(buffer), outLen));
	EXPECT_EQ(outLen, 0);
	EXPECT_EQ(buffer[0], '\0');  // NUL should be written
}

TEST(HistoryEncoding, EncodeUInt16KnownVector)
{
	uint16_t v = 0x0102;
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt16(&v, 1, buffer, sizeof(buffer), outLen));
	EXPECT_EQ(outLen, 4);  // 2 bytes encodes to 4 chars with 1 = padding
	EXPECT_TRUE(buffer[outLen - 1] == '=');  // Should have padding
	EXPECT_EQ(buffer[outLen], '\0');  // NUL should be written after
}

TEST(HistoryEncoding, EncodeUInt16Zero)
{
	uint16_t v = 0;
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt16(&v, 1, buffer, sizeof(buffer), outLen));
	uint16_t decoded;
	EXPECT_TRUE(DecodeUInt16(buffer, outLen, &decoded, 1));
	EXPECT_EQ(decoded, 0);
}

TEST(HistoryEncoding, EncodeUInt16Max)
{
	uint16_t v = 65535;
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt16(&v, 1, buffer, sizeof(buffer), outLen));
	uint16_t decoded;
	EXPECT_TRUE(DecodeUInt16(buffer, outLen, &decoded, 1));
	EXPECT_EQ(decoded, 65535);
}

TEST(HistoryEncoding, EncodeUInt16AllPatterns)
{
	uint16_t v[3] = { 0x0000, 0x0102, 0xFFFF };
	char buffer[32];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt16(v, 3, buffer, sizeof(buffer), outLen));
	uint16_t decoded[3];
	EXPECT_TRUE(DecodeUInt16(buffer, outLen, decoded, 3));
	EXPECT_EQ(decoded[0], 0x0000);
	EXPECT_EQ(decoded[1], 0x0102);
	EXPECT_EQ(decoded[2], 0xFFFF);
}

TEST(HistoryEncoding, EncodeUInt16CapacityExactly)
{
	uint16_t v = 0x0102;
	char buffer[4];  // Exactly needed
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt16(&v, 1, buffer, 4, outLen));
	EXPECT_EQ(outLen, 4);
	// No NUL space available
}

TEST(HistoryEncoding, EncodeUInt16CapacityTooSmall)
{
	uint16_t v = 0x0102;
	char buffer[4];
	uint32_t outLen = 99;
	memset(buffer, 0xCC, sizeof(buffer));
	// Needed is 4, capacity is 3
	EXPECT_FALSE(EncodeUInt16(&v, 1, buffer, 3, outLen));
	EXPECT_EQ(outLen, 0);
	// Buffer should be untouched
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_EQ(static_cast<unsigned char>(buffer[i]), 0xCC);
	}
}

TEST(HistoryEncoding, EncodeUInt16VariousLengths)
{
	std::vector<uint16_t> test = { 1, 2, 3, 4, 5 };
	for (size_t n = 0; n <= test.size(); ++n)
	{
		uint32_t needed = EncodedLength(n * sizeof(uint16_t)) + 1;
		std::vector<char> buffer(needed);
		uint32_t outLen = 0;
		EXPECT_TRUE(EncodeUInt16(test.data(), n, buffer.data(), buffer.size(), outLen)) << "Failed for n=" << n;
		std::vector<uint16_t> decoded(n);
		EXPECT_TRUE(DecodeUInt16(buffer.data(), outLen, decoded.data(), n)) << "Failed for n=" << n;
		for (size_t i = 0; i < n; ++i)
		{
			EXPECT_EQ(decoded[i], test[i]) << "Mismatch at index " << i;
		}
	}
}

TEST(HistoryEncoding, EncodeUInt16LargeArray)
{
	std::vector<uint16_t> v(100000);
	for (size_t i = 0; i < v.size(); ++i)
	{
		v[i] = static_cast<uint16_t>(i % 65536);
	}
	uint32_t needed = EncodedLength(v.size() * sizeof(uint16_t)) + 1;
	std::vector<char> buffer(needed);
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt16(v.data(), v.size(), buffer.data(), buffer.size(), outLen));
	std::vector<uint16_t> decoded(v.size());
	EXPECT_TRUE(DecodeUInt16(buffer.data(), outLen, decoded.data(), v.size()));
	for (size_t i = 0; i < v.size(); ++i)
	{
		EXPECT_EQ(decoded[i], v[i]);
	}
}

// ======== UInt8 Tests ========

TEST(HistoryEncoding, EncodeUInt8Empty)
{
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt8(nullptr, 0, buffer, sizeof(buffer), outLen));
	EXPECT_EQ(outLen, 0);
	EXPECT_EQ(buffer[0], '\0');  // NUL should be written
}

TEST(HistoryEncoding, EncodeUInt8Zero)
{
	uint8_t v = 0;
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt8(&v, 1, buffer, sizeof(buffer), outLen));
	uint8_t decoded;
	EXPECT_TRUE(DecodeUInt8(buffer, outLen, &decoded, 1));
	EXPECT_EQ(decoded, 0);
}

TEST(HistoryEncoding, EncodeUInt8Max)
{
	uint8_t v = 255;
	char buffer[16];
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt8(&v, 1, buffer, sizeof(buffer), outLen));
	uint8_t decoded;
	EXPECT_TRUE(DecodeUInt8(buffer, outLen, &decoded, 1));
	EXPECT_EQ(decoded, 255);
}

TEST(HistoryEncoding, EncodeUInt8CapacityExactly)
{
	uint8_t v = 42;
	char buffer[4];  // Exactly needed
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt8(&v, 1, buffer, 4, outLen));
	EXPECT_EQ(outLen, 4);
	// No NUL space available
}

TEST(HistoryEncoding, EncodeUInt8CapacityTooSmall)
{
	uint8_t v = 42;
	char buffer[4];
	uint32_t outLen = 99;
	memset(buffer, 0xCC, sizeof(buffer));
	// Needed is 4, capacity is 3
	EXPECT_FALSE(EncodeUInt8(&v, 1, buffer, 3, outLen));
	EXPECT_EQ(outLen, 0);
	// Buffer should be untouched
	for (int i = 0; i < 4; ++i)
	{
		EXPECT_EQ(static_cast<unsigned char>(buffer[i]), 0xCC);
	}
}

TEST(HistoryEncoding, EncodeUInt8VariousLengths)
{
	std::vector<uint8_t> test = { 1, 2, 3, 4, 5 };
	for (size_t n = 0; n <= test.size(); ++n)
	{
		uint32_t needed = EncodedLength(n * sizeof(uint8_t)) + 1;
		std::vector<char> buffer(needed);
		uint32_t outLen = 0;
		EXPECT_TRUE(EncodeUInt8(test.data(), n, buffer.data(), buffer.size(), outLen)) << "Failed for n=" << n;
		std::vector<uint8_t> decoded(n);
		EXPECT_TRUE(DecodeUInt8(buffer.data(), outLen, decoded.data(), n)) << "Failed for n=" << n;
		for (size_t i = 0; i < n; ++i)
		{
			EXPECT_EQ(decoded[i], test[i]) << "Mismatch at index " << i;
		}
	}
}

TEST(HistoryEncoding, EncodeUInt8LargeArray)
{
	std::vector<uint8_t> v(100000);
	for (size_t i = 0; i < v.size(); ++i)
	{
		v[i] = static_cast<uint8_t>(i % 256);
	}
	uint32_t needed = EncodedLength(v.size() * sizeof(uint8_t)) + 1;
	std::vector<char> buffer(needed);
	uint32_t outLen = 0;
	EXPECT_TRUE(EncodeUInt8(v.data(), v.size(), buffer.data(), buffer.size(), outLen));
	std::vector<uint8_t> decoded(v.size());
	EXPECT_TRUE(DecodeUInt8(buffer.data(), outLen, decoded.data(), v.size()));
	for (size_t i = 0; i < v.size(); ++i)
	{
		EXPECT_EQ(decoded[i], v[i]);
	}
}

// ======== Decode Error Tests ========

TEST(HistoryEncoding, DecodeFloat32InvalidBase64)
{
	const char* invalid = "!!!@";  // Invalid base64 characters
	float out;
	EXPECT_FALSE(DecodeFloat32(invalid, 4, &out, 1));
}

TEST(HistoryEncoding, DecodeFloat32WrongPadding)
{
	const char* wrongPadding = "AAAA=";  // Invalid padding
	float out;
	EXPECT_FALSE(DecodeFloat32(wrongPadding, 5, &out, 1));
}

TEST(HistoryEncoding, DecodeFloat32WrongLength)
{
	float v = 1.0f;
	char buffer[16];
	uint32_t outLen = 0;
	EncodeFloat32(&v, 1, buffer, sizeof(buffer), outLen);
	// Try to decode as 2 elements (16 bytes)
	std::vector<float> out(2);
	EXPECT_FALSE(DecodeFloat32(buffer, outLen, out.data(), 2));
}

TEST(HistoryEncoding, DecodeFloat32TruncatedInput)
{
	float v = 1.0f;
	char buffer[16];
	uint32_t outLen = 0;
	EncodeFloat32(&v, 1, buffer, sizeof(buffer), outLen);
	// Decode truncated string
	float out;
	EXPECT_FALSE(DecodeFloat32(buffer, 2, &out, 1));
}

TEST(HistoryEncoding, DecodeFloat32OutputUntouchedOnFailure)
{
	float out = 99.0f;
	const char* invalid = "!!!@";
	EXPECT_FALSE(DecodeFloat32(invalid, 4, &out, 1));
	EXPECT_EQ(out, 99.0f);  // Should not be modified
}

TEST(HistoryEncoding, DecodeUInt16InvalidBase64)
{
	const char* invalid = "!!!@";
	uint16_t out;
	EXPECT_FALSE(DecodeUInt16(invalid, 4, &out, 1));
}

TEST(HistoryEncoding, DecodeUInt16WrongLength)
{
	uint16_t v = 42;
	char buffer[16];
	uint32_t outLen = 0;
	EncodeUInt16(&v, 1, buffer, sizeof(buffer), outLen);
	uint16_t out;
	EXPECT_FALSE(DecodeUInt16(buffer, outLen, &out, 2));
}

TEST(HistoryEncoding, DecodeUInt8InvalidBase64)
{
	const char* invalid = "!!!@";
	uint8_t out;
	EXPECT_FALSE(DecodeUInt8(invalid, 4, &out, 1));
}

TEST(HistoryEncoding, DecodeUInt8WrongLength)
{
	uint8_t v = 42;
	char buffer[16];
	uint32_t outLen = 0;
	EncodeUInt8(&v, 1, buffer, sizeof(buffer), outLen);
	uint8_t out;
	EXPECT_FALSE(DecodeUInt8(buffer, outLen, &out, 2));
}

// ======== EncodedLength Tests ========

TEST(HistoryEncoding, EncodedLengthVariousByteCount)
{
	EXPECT_EQ(EncodedLength(0), 0);
	EXPECT_EQ(EncodedLength(1), 4);
	EXPECT_EQ(EncodedLength(2), 4);
	EXPECT_EQ(EncodedLength(3), 4);
	EXPECT_EQ(EncodedLength(4), 8);
	EXPECT_EQ(EncodedLength(5), 8);
	EXPECT_EQ(EncodedLength(6), 8);
	EXPECT_EQ(EncodedLength(7), 12);
}

// ======== Little-Endian Verification ========

TEST(HistoryEncoding, DecodeFloat32VerifyLittleEndian)
{
	// Manually construct a little-endian encoded 1.0f (0x3F800000)
	// Bytes: 00 00 80 3F (little-endian)
	// Base64: "AACAPw=="
	std::string encoded = "AACAPw==";
	float out;
	EXPECT_TRUE(DecodeFloat32(encoded.c_str(), encoded.size(), &out, 1));
	EXPECT_EQ(out, 1.0f);
}

TEST(HistoryEncoding, DecodeUInt16VerifyLittleEndian)
{
	// uint16 0x0102 in little-endian: 02 01
	uint16_t v = 0x0102;
	char buffer[16];
	uint32_t outLen = 0;
	EncodeUInt16(&v, 1, buffer, sizeof(buffer), outLen);
	uint16_t decoded;
	EXPECT_TRUE(DecodeUInt16(buffer, outLen, &decoded, 1));
	EXPECT_EQ(decoded, 0x0102);
}
