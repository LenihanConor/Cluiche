#pragma once

// HistoryEncoding: base64 encoders/decoders for numeric arrays, little-endian regardless of host.
// Used by DiaCellHistory export and live queries.

#include <stdint.h>

namespace Dia::CellHistory
{
	// Encode float32 array to little-endian base64.
	// Writes base64 text to the caller-provided buffer `out`.
	// Returns true on success, false if capacity is too small.
	// On failure, `out` is untouched and `outLen` is set to 0.
	// On success, `outLen` is the number of base64 characters written (no NUL terminator added,
	// but if capacity allows, a NUL is written at out[outLen] for convenience).
	// For an empty array, returns true with outLen = 0.
	// Required capacity = EncodedLength(n * sizeof(float)) + 1 (for NUL).
	bool EncodeFloat32(const float* v, uint32_t n, char* out, uint32_t capacity, uint32_t& outLen);

	// Encode uint16 array to little-endian base64.
	// Signature and semantics identical to EncodeFloat32.
	// Required capacity = EncodedLength(n * sizeof(uint16_t)) + 1 (for NUL).
	bool EncodeUInt16(const uint16_t* v, uint32_t n, char* out, uint32_t capacity, uint32_t& outLen);

	// Encode uint8 array to little-endian base64.
	// Signature and semantics identical to EncodeFloat32.
	// Required capacity = EncodedLength(n * sizeof(uint8_t)) + 1 (for NUL).
	bool EncodeUInt8(const uint8_t* v, uint32_t n, char* out, uint32_t capacity, uint32_t& outLen);

	// Decode base64 to float32 array.
	// n is the number of elements expected (float32 = 4 bytes each).
	// Returns false if base64 is invalid, length doesn't match n, or padding is wrong.
	// On failure, out is untouched.
	bool DecodeFloat32(const char* text, uint32_t textLen, float* out, uint32_t n);

	// Decode base64 to uint16 array.
	// n is the number of elements expected (uint16 = 2 bytes each).
	// Returns false on invalid base64, length mismatch, or bad padding.
	// On failure, out is untouched.
	bool DecodeUInt16(const char* text, uint32_t textLen, uint16_t* out, uint32_t n);

	// Decode base64 to uint8 array.
	// n is the number of elements expected (uint8 = 1 byte each).
	// Returns false on invalid base64, length mismatch, or bad padding.
	// On failure, out is untouched.
	bool DecodeUInt8(const char* text, uint32_t textLen, uint8_t* out, uint32_t n);

	// Compute the base64 encoded length for byteCount bytes (with padding).
	uint32_t EncodedLength(uint32_t byteCount);
}
