#pragma once

// SnapshotCellFile: the per-cell copies of a snapshot (format `dia.cell_snapshot` v1, JSON), written next to the game's
// state file as `<store folder>/snapshot_<name>.cells.json`.
//
// SnapshotCellData is the in-memory model: header, grid, and up to 64 layers. Float layers hold little-endian uint16
// codes quantised over the schema min..max with CellRing's own functions (error at most range / 65,535; NaN is code
// 0xFFFF and reads back as NaN); category layers hold uint8 ids (255 = NaN/none) plus optional enum value names.
// The file stores each payload as base64 (HistoryEncoding).
// Pure with respect to time and world: the creation time and the world name are supplied by the caller.
// Same-PU contract; no singletons; no STL in this header (PD-004). Errors are reported as text in a caller buffer.

#include <DiaCore/CRC/StringCRC.h>
#include <stdint.h>

namespace Dia::CellHistory { class CellHistoryRecorder; }
namespace Dia::CellLayers { class CellLayerRegistry; }
namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellHistory
{
	struct SnapshotHeader
	{
		char		mName[33] = {};
		uint64_t	mRunId = 0;			// written as 16 hex digits
		uint32_t	mSample = 0;		// index of the most recent sample when the snapshot was taken
		char		mLabel[32] = {};
		int64_t		mCreatedUnix = 0;	// seconds
		char		mWorld[64] = {};	// may be empty
	};

	struct SnapshotGridInfo
	{
		char		mId[64] = {};
		uint32_t	mWidth = 0;
		uint32_t	mHeight = 0;
		bool		SameAs(const SnapshotGridInfo& o) const;
	};

	enum class SnapshotLayerKind : uint8_t { Float, Category };

	class SnapshotCellData;

	class SnapshotLayer
	{
	public:
		static constexpr uint32_t kIdChars = 64;
		static constexpr uint32_t kMaxEnumValues = 256;
		static constexpr uint32_t kEnumNameChars = 48;

		SnapshotLayer() = default;
		~SnapshotLayer() { Release(); }
		SnapshotLayer(const SnapshotLayer&) = delete;
		SnapshotLayer& operator=(const SnapshotLayer&) = delete;

		char				mId[kIdChars] = {};
		SnapshotLayerKind	mKind = SnapshotLayerKind::Float;
		float				mMin = 0.f;			// Float only
		float				mMax = 1.f;

		uint32_t		EnumCount() const { return mEnumCount; }
		const char*		EnumName(uint32_t i) const { return i < mEnumCount ? mEnumNames + size_t(i) * kEnumNameChars : ""; }
		void			SetEnumName(uint32_t i, const char* name);
		const uint8_t*	Payload() const { return mPayload; }		// little-endian uint16 codes, or uint8 ids
		uint8_t*		MutablePayload() { return mPayload; }
		uint32_t		PayloadBytes() const { return mPayloadBytes; }

	private:
		friend class SnapshotCellData;
		void Release();
		uint8_t*	mPayload = nullptr;
		uint32_t	mPayloadBytes = 0;
		char*		mEnumNames = nullptr;
		uint32_t	mEnumCount = 0;
	};

	class SnapshotCellData
	{
	public:
		static constexpr uint32_t kMaxLayers = 64;

		SnapshotCellData() = default;
		SnapshotCellData(const SnapshotCellData&) = delete;
		SnapshotCellData& operator=(const SnapshotCellData&) = delete;

		SnapshotHeader		mHeader;
		SnapshotGridInfo	mGrid;

		uint32_t			LayerCount() const { return mLayerCount; }
		uint32_t			CellCount() const { return mGrid.mWidth * mGrid.mHeight; }
		const SnapshotLayer*	LayerAt(uint32_t i) const { return i < mLayerCount ? &mLayers[i] : nullptr; }
		SnapshotLayer*			LayerAt(uint32_t i) { return i < mLayerCount ? &mLayers[i] : nullptr; }
		const SnapshotLayer*	FindLayer(Dia::Core::StringCRC id) const;
		// Appends a layer with a zeroed payload for CellCount() cells (grid must be set first). Null when full or the
		// grid is empty. enumCount (category only) reserves enum name slots (<= 256).
		SnapshotLayer*		AddLayer(const char* id, SnapshotLayerKind kind, float mn, float mx, uint32_t enumCount);
		void				Clear();	// drops all layers and resets header and grid

	private:
		SnapshotLayer	mLayers[kMaxLayers];
		uint32_t		mLayerCount = 0;
	};

	class SnapshotCellFile
	{
	public:
		// `<folder>/snapshot_<name>.cells.json`. False for an invalid name, a null folder, or a small buffer.
		static bool Path(const char* folder, const char* name, char* buf, uint32_t capacity);

		// Captures a quantised copy of EVERY recorded layer of the recorder from the layers' LIVE `read` values (not the
		// ring). Float layers without a schema range cannot be quantised and are skipped with a Warning (log channel HIS).
		// Header: sample = SampleCount() - 1 (0 when none), label of that sample, the recorder's run id.
		static bool BuildFromLive(const CellHistoryRecorder& rec, const Dia::CellLayers::CellLayerRegistry& registry,
			const Dia::ValueSchema::ValueSchemaService* schema, const char* name, int64_t createdUnix, const char* world,
			SnapshotCellData& out, char* err, uint32_t errCapacity);

		// Writes `<path>.tmp` then renames it over `path`. On any failure the temp file this call created is removed and an
		// existing `path` is left untouched; `err` names the reason.
		static bool WriteAtomic(const SnapshotCellData& data, const char* path, char* err, uint32_t errCapacity);

		// Reads and validates (format, version, fields, sizes, base64, kinds, <= 64 layers). `out` is cleared on failure.
		static bool Read(const char* path, SnapshotCellData& out, char* err, uint32_t errCapacity);

		// Validates and returns only the header and grid (no payload decoding), for list and restore checks.
		static bool ReadHeaderOnly(const char* path, SnapshotHeader& header, SnapshotGridInfo& grid, char* err, uint32_t errCapacity);

		// Float layer: dequantised values; category layer: the id as a float (255 stays 255). False for an unknown layer
		// or n != CellCount().
		static bool Dequantise(const SnapshotCellData& data, Dia::Core::StringCRC layer, float* out, uint32_t n);
	};
}
