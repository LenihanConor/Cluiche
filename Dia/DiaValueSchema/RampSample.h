#pragma once

// RampSample: sampling helpers over RampData. Colours are 0xRRGGBBAA.

#include <DiaValueSchema/RampData.h>

namespace Dia::ValueSchema
{
	// Samples a CONTINUOUS ramp at t. t is clamped to [0,1]; NaN is treated as 0. Linear interpolation in sRGB,
	// per channel, rounded to nearest; alpha is always 255. Exact at stops.
	// A categorical ramp is not sampleable: SampleRamp returns its first colour (documented fallback).
	// A ramp with no stops/colours returns opaque black 0x000000FF.
	uint32_t SampleRamp(const RampData& ramp, float t);

	// Categorical colour by index, wrapping modulo the colour count. For a continuous ramp returns SampleRamp(ramp, 0).
	uint32_t RampColourAt(const RampData& ramp, unsigned index);
}
