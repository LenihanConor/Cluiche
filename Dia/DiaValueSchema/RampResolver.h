#pragma once

// RampResolver: resolves the ramp / palette a schema entry asks for (`ui.ramp`, `ui.delta_ramp`, enum colours).
//
// Deliberate deviation from the spec's free-function signatures: resolution is a class so the once-per-key
// warning memory lives HERE (pimpl set of full keys), not in the immutable ValueSchemaService, which stays
// lock-free. Instances are independent (no global state). Not thread-safe: one resolver per thread/consumer.
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

	// value in [min,max] -> t in [0,1], clamped. NaN -> 0. Zero-width (or inverted) range -> 0.
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
		// Id from `ui.delta_ramp` (default diverging.blue_red). Range is symmetric: [-maxAbs, +maxAbs]; maxAbs <= 0
		// or NaN uses 1.0, so value 0 maps to t = 0.5.
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
