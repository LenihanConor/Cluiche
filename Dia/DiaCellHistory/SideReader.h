#pragma once

// SideReader: read-only helper that reads ONE side of a delta for one layer. Sides are `now` (the layer's own live
// `read`, exact), `sample:<i>` (the dequantised per-cell ring of the recorder) and `snapshot:<name>` (parsed, but
// reading reports Unavailable until snapshot support lands). A side with no data reports NoData; the reader NEVER
// falls back to live values to fill a gap, and never goes through the registry's IViewValueSource.
// Same-PU contract as the recorder; no locking, no singletons, no allocation.

#include <DiaCore/Strings/String64.h>
#include <cstdint>

namespace Dia::CellLayers { struct CellLayerDesc; }

namespace Dia::CellHistory
{
	class CellHistoryRecorder;

	enum class SideKind : uint8_t { Now, Sample, Snapshot };

	struct Side
	{
		SideKind						kind = SideKind::Now;
		uint32_t						sample = 0;		// Sample only
		Dia::Core::Containers::String64	snapshot;		// Snapshot only
	};

	struct SideParseResult
	{
		static constexpr uint32_t kErrorChars = 128;
		bool	ok = false;
		char	error[kErrorChars] = {};
	};

	// "now", "sample:<non-negative integer>", "snapshot:<name>" (name 1..63 chars). Anything else sets err and returns false.
	bool ParseSide(const char* text, Side& out, SideParseResult& err);
	bool SidesEqual(const Side& a, const Side& b);

	enum class SideReadStatus : uint8_t { Ok, NoData, Unavailable };	// Unavailable: snapshot sides are not supported yet

	class SideReader
	{
	public:
		explicit SideReader(const CellHistoryRecorder& recorder) : mRecorder(recorder) {}

		// Fills out[0..n) on Ok. On NoData / Unavailable out is not guaranteed to be written.
		SideReadStatus Read(const Side& side, const Dia::CellLayers::CellLayerDesc& layer, float* out, uint32_t n) const;

	private:
		const CellHistoryRecorder& mRecorder;
	};
}
