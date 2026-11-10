#pragma once

// RampData: one colour ramp loaded from a dia.ramps file (see RampLoader). Plain data, cheap to copy.
//
// Colour packing: RGBA8 as 0xRRGGBBAA in a uint32_t (red in the top byte, alpha in the low byte).
// Colours parsed from "#rrggbb" always have alpha 0xFF.

#include <cstdint>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String64.h>

namespace Dia::ValueSchema
{
	constexpr unsigned kMaxRampStops = 16;
	constexpr unsigned kMaxCategoricalColours = 32;

	enum class RampKind { Continuous, Categorical };

	struct RampStop
	{
		float		mT = 0.0f;		// 0..1, strictly increasing across a ramp's stops
		uint32_t	mRgba8 = 0;		// 0xRRGGBBAA
	};

	struct RampData
	{
		Dia::Core::StringCRC				mId;		// CRC of mName
		Dia::Core::Containers::String64		mName;		// diagnostics
		RampKind							mKind = RampKind::Continuous;
		RampStop							mStops[kMaxRampStops];				// Continuous: first mStopCount valid
		unsigned							mStopCount = 0;
		uint32_t							mColours[kMaxCategoricalColours] = {};	// Categorical: first mColourCount valid (0xRRGGBBAA)
		unsigned							mColourCount = 0;
	};

	// Parses exactly "#rrggbb" (6 hex digits, either case). On success writes 0xRRGGBBFF to rgba8.
	// Returns false (rgba8 untouched) for null, missing '#', wrong length or non-hex characters.
	bool ParseHexColour(const char* text, uint32_t& rgba8);
}
