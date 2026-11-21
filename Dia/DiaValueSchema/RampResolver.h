#pragma once

// RampResolver: resolves the ramp / palette a schema entry asks for (`ui.ramp`, `ui.delta_ramp`, enum colours).
//
// Deliberate deviation from the spec's free-function signatures: resolution is a class so the once-per-key
// warning memory lives HERE (pimpl set of full keys), not in the immutable ValueSchemaService, which stays
// lock-free. Instances are independent (no global state). Not thread-safe: one resolver per thread/consumer (the shared
// const ValueSchemaService may be used by many resolvers on many threads).
//
// ResolveRamp range: min/max are copied from the entry as floats; doubles beyond +-FLT_MAX saturate at +-FLT_MAX
// (never +-inf); a NaN bound is carried as NaN (RampT then gives 0). `ui.ramp` / `ui.delta_ramp` that are not strings
// (and a `ui` that is not an object) mean "not requested".
//
// Fallbacks (never crash):
//  - `ui.ramp` absent -> sequential.grey, no warning.
//  - unknown id, or id of a categorical ramp on a float entry -> sequential.grey + ONE Warning ("VSC") per key.
//  - sequential.grey itself missing (no ramps loaded) -> built-in 2-stop black->white ramp (counted as a fallback).
//  - every fallback increments FallbackCount() and the bound metric Counter (if any).

#include <DiaValueSchema/RampData.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

namespace Dia::Observation::Metric { class Counter; }

namespace Dia::ValueSchema
{
	struct ResolvedRamp
	{
		RampData	mRamp;
		float		mMin = 0.0f;
		float		mMax = 1.0f;
		bool		mHasRange = false;	// false when the entry lacks either bound (then 0..1 is used)
		bool		mFellBack = false;	// the requested ramp was unknown/unusable
	};

	struct PaletteEntry
	{
		Dia::Core::Containers::String64	mId;
		uint32_t						mRgba8 = 0;
	};

	struct ResolvedPalette
	{
		ValueList<PaletteEntry>	mEntries;
		uint32_t				mFallbackRgba8 = 0x808080FF;	// neutral grey
		bool					mWrapped = false;				// colour-less values exceeded the categorical count
	};

	// value in [min,max] -> t in [0,1], clamped; +-inf values clamp to 1 / 0. NEVER returns NaN: NaN value, a NaN or
	// infinite bound, and a zero-width or inverted range all give 0. Computed in double, so a range whose width exceeds
	// FLT_MAX (e.g. [-3e38, 3e38]) still scales correctly.
	float RampT(const ResolvedRamp& ramp, float value);

	class RampResolver
	{
	public:
		explicit RampResolver(const ValueSchemaService& service);
		~RampResolver();
		RampResolver(const RampResolver&) = delete;
		RampResolver& operator=(const RampResolver&) = delete;

		ResolvedRamp	ResolveRamp(const ValueInfo& info);
		// Colours for an enum entry: given colour, else categorical.12 by value order (wraps modulo its count).
		// Non-enum entry -> empty palette.
		ResolvedPalette	ResolvePalette(const ValueInfo& info);
		// Id from `ui.delta_ramp` (default diverging.blue_red). Range is symmetric: [-maxAbs, +maxAbs]; maxAbs <= 0,
		// NaN or +-inf uses 1.0, so value 0 maps to t = 0.5.
		ResolvedRamp	ResolveDeltaRamp(const ValueInfo& info, float maxAbs);

		unsigned int UnknownRampWarningCount() const	{ return mUnknownWarnings; }
		unsigned int WrapWarningCount() const			{ return mWrapWarnings; }
		unsigned int FallbackCount() const				{ return mFallbackCount; }

		void BindFallbackCounter(Dia::Observation::Metric::Counter* counter) { mCounter = counter; }

	private:
		ResolvedRamp	ResolveNamed(const ValueInfo& info, const char* uiKey, const char* defaultId);
		void			NoteFallback();
		bool			MarkWarned(const char* tag, const ValueInfo& info);

		struct Impl;
		Impl*			mImpl;
		const ValueSchemaService& mService;
		unsigned int	mUnknownWarnings = 0;
		unsigned int	mWrapWarnings = 0;
		unsigned int	mFallbackCount = 0;
		Dia::Observation::Metric::Counter* mCounter = nullptr;
	};
}
