#pragma once

// ValueSchemaWriter: WriteDefaultsInPlace (DiaAttribute parameter-files, ACs 22-25).
//
// Rewrites the `default` values of named sidecar entries and leaves every other byte of the text alone
// (whitespace, line endings, comments, key order, other entries, labels). It scans the text token by token; it
// does not parse into a document and re-serialise, so nothing but the replaced `default` tokens can change.
//
//   - A scalar entry is addressed by DefaultValue{mKey}; a map entry's object default per key by
//     DefaultValue{mKey, mMapKey}, one DefaultValue per map key to change.
//   - A value equal to the one already written (numerically, after parsing the existing token) keeps the original
//     token, so an unchanged write is byte-identical even when the file spells a number as `5e-1` or `3.0`.
//   - Changed numbers are written in the shortest form that reads back to the same value: Float (a double) and
//     Float32 (a float, so 0.1f is written 0.1, not 0.10000000149011612). Float and Float32 always contain a '.'
//     or exponent; Int is written without one. -0.0 is kept distinct from 0.0.
//   - Fails (returns false, `out` left empty, `err` naming the key) for an unknown entry key, an entry with no
//     `default`, a map key that is not in the entry's object default, a map value on a non-object default or a
//     scalar value on an object default, a duplicate target, a non-finite number, or text that is not parseable.
//
// `out` holds the new text followed by one terminating '\0' (Size() == length + 1); empty on failure.
// No STL in the signature (module convention): ValueList<char>.

#include <DiaValueSchema/ValueInfo.h>
#include <stdint.h>

namespace Dia::ValueSchema
{
	enum class DefaultKind : uint8_t
	{
		Float,		// mNumber, shortest round-trip double
		Float32,	// mNumber (a float value widened), shortest round-trip float
		Int,		// mNumber, written as an integer
		Bool,		// mNumber != 0
		String		// mString (enum id or string), JSON-escaped
	};

	struct DefaultValue
	{
		const char*		mKey = nullptr;		// sidecar entry key
		const char*		mMapKey = nullptr;	// null for a scalar default; the map key for a map entry's object default
		DefaultKind		mKind = DefaultKind::Float;
		double			mNumber = 0.0;
		const char*		mString = nullptr;
	};

	struct WriteError
	{
		Dia::Core::Containers::String128	mKey;		// entry key, or "<key>.<mapKey>" for a map key
		Dia::Core::Containers::String512	mReason;
	};

	bool WriteDefaultsInPlace(const char* sidecarText, const DefaultValue* values, uint32_t count,
		ValueList<char>& out, WriteError& err);
}
