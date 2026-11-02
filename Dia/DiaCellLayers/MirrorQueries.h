#pragma once
#ifdef DIA_DEBUG

// MirrorQueries: the pull side of the editor mirror (Feature 5). Plain logic class, separate from registration:
// RegisterMirrorQueries(IMirrorQueryRegistrar&, MirrorQueries&) wires it (host wiring calls it). The registrar is a seam so this
// library stays free of the host: the host supplies one that forwards to the sim-hosted query service (SimQueryHostModule). Read-only: never mutates the viewer,
// registry or selection.
//
// Queries answered here:
//   cells.layer.list  (no args) -> { layersVersion, grids:[{id:"<grid id>"|"grid.<i>", index, width, height, layers:[{id, schemaKey, group,
//                       kind:"float"|"category", hotkey}]}], overlays:[{id, label, group}] }
//                       Same data source as the DiaAPI command of that name (CellLayerViewerDomain::ListLayers); this adds
//                       layersVersion, grid ids, schemaKey and hotkey. hotkey is "" when unbound or there is no schema.
//   cells.select.get  { offset?, count? } -> { version, hash (16 hex), total, offset, indices:[..] } of the active grid's
//                       selection, ascending, at most kMaxSelectionChunk (1,000) per call (a larger count is clamped; Warning on CEL).
//                       Bad args (non-numeric, negative, offset > total) -> { "error": "<message>" }. No active grid: empty.
//  cells.layer.values { layer, offset?, count? } -> { layer, version, gridId, width, height, offset, count, total,
//                       encoding:"f32le-base64", data } : little-endian float32 per cell [offset, offset+count), base64. Values
//                       come from the viewer's LayerReader (what the viewer shows); version is the layer version. At most
//                       MaxValuesChunk() cells per call (a larger count is clamped; Warning on CEL for every clamp). count
//                       defaults to min(total-offset, chunk). offset == total gives empty data. Unknown layer, bad args or
//                       offset > total -> { "error": "<message>" }.
//
//  cells.select.stats { layers?: [ids] | "all" } -> the Feature 4 stats JSON (InspectorModel::BuildStatsJson, same per-layer shape
//                       as cells.inspect) for the active grid's selection, plus top-level `count`, `version`, `hash` (16 hex) of the
//                       selection. `layers` omitted = the inspector's shown set (base first; the Feature 4 default); "all" = every
//                       layer of the active grid; an array = those layers in order. A bad value (not an array / "all", non-string or
//                       empty elements, empty array, unknown layer, layer of another grid) or no active grid / unsynced inspector ->
//                       { "error": "<message>" }. Aggregation runs at most once per frame: the result is cached on (frame, selection
//                       identity/version/count, layer set, each layer's version, pin state, shown set) and reused until the host calls
//                       NotifyFrame() or one of those changes. StatsComputeCount() counts aggregation passes (test-visible).
//
// Threading: handlers run on SimPU (sim-hosted queries), the same PU as the viewer, so they take no locks. Register and
// Unregister must also be called on SimPU. Borrowed references must outlive the object.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerReader.h>

#include <functional>
#include <stdint.h>
#include <vector>

namespace Dia::Observation::Metric { class Counter; }

namespace Dia::CellLayers
{
	// Shared grid id: CellGridDesc::id when non-empty, else "grid.<index>" (index = position in CellLayerRegistry::GridAt).
	// Used by the view-state topic and the queries.
	namespace MirrorGridId
	{
		constexpr uint32_t kBufferSize = 72;	// StringCRC capacity (64) + slack for the "grid.<index>" fallback
		uint32_t	IndexOf(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid);	// 0 when not found
		void		Format(uint32_t index, char (&out)[kBufferSize]);	// fallback form "grid.<index>"
		void		Format(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid, char (&out)[kBufferSize]);
	}

	class MirrorQueries
	{
	public:
		static const uint32_t kMaxSelectionChunk = 1000;	// indices per cells.select.get call (reply stays under the 16 KB sim-hosted cap)
		static const uint32_t kStatsCacheCapacity = 8;	// stats cache entries per frame (round-robin eviction)
		static const uint32_t kMaxValuesChunk = 1000;		// cells per cells.layer.values call: 4,000 bytes -> about 5.3 KB of base64

		// schema may be null (no hotkeys). Registry, viewer and schema are borrowed.
		MirrorQueries(const CellLayerRegistry& registry, const CellLayerViewerDomain& viewer, const Dia::ValueSchema::ValueSchemaService* schema);

		Json::Value	LayerList() const;
		Json::Value	SelectionGet(const Json::Value& args) const;
		Json::Value	LayerValues(const Json::Value& args) const;

		Json::Value	SelectionStats(const Json::Value& args) const;

		// Host hook: call once per frame (after the viewer's Update). Ends the current frame's stats cache entry.
		void		NotifyFrame();
		uint32_t	StatsComputeCount() const { return mStatsComputeCount; }	// aggregation passes performed so far

		// Observation. Called by the registered handlers once per served query (not by the query methods themselves, so direct
		// calls in tests do not count): bumps dia.celllayers.mirror.queries.<kind> and .bytes_served (serialized reply size) and
		// logs Debug on CEL (query name, bytes). Metrics are registered in the constructor (MetricRegistry::Instance()).
		enum class Kind { List, Values, SelectGet, Stats };
		void		RecordServed(Kind kind, const Json::Value& reply) const;

		// Test seam: lowers the per-call cell limit (clamped to [1, kMaxValuesChunk]) so chunking is testable on small grids.
		void		SetMaxValuesChunk(uint32_t cells);
		uint32_t	MaxValuesChunk() const { return mMaxValuesChunk; }

	private:
		const CellLayerRegistry&		mRegistry;
		const CellLayerViewerDomain&	mViewer;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		uint32_t					mMaxValuesChunk = kMaxValuesChunk;
		mutable LayerReader			mReader;	// SimPU only

		// Stats cache (SimPU only).
		uint32_t					mFrame = 0;
		// Fixed-size cache key. Worst case: 8 header words + kMaxShown shown ids + 2 words per layer (kMaxLayers) = 296.
		struct StatsKey
		{
			static const uint32_t kCapacity = 320;
			uint64_t	words[kCapacity];
			uint32_t	count = 0;
			void Push(uint64_t w) { if (count < kCapacity) { words[count++] = w; } }
			bool operator==(const StatsKey& o) const
			{
				if (count != o.count) { return false; }
				for (uint32_t i = 0; i < count; ++i) { if (words[i] != o.words[i]) { return false; } }
				return true;
			}
		};
		struct StatsCacheEntry
		{
			bool					valid = false;
			uint32_t				frame = 0;
			StatsKey				key;
			Json::Value				result;
		};
		mutable StatsCacheEntry		mStatsCache[kStatsCacheCapacity];
		mutable uint32_t			mStatsCacheNext = 0;	// round-robin insert slot
		mutable uint32_t			mStatsComputeCount = 0;

		Dia::Observation::Metric::Counter*	mMetricQueries[4] = { nullptr, nullptr, nullptr, nullptr };	// by Kind
		Dia::Observation::Metric::Counter*	mMetricBytes = nullptr;
	};

	// Seam to whatever hosts handlers on SimPU (the host app supplies one forwarding to SimQueryHostModule). Read-only handlers
	// (the mirror queries) use Register; handlers that change state (the viewer commands, CellLayerViewerDomain::RegisterCommands)
	// use RegisterCommand. A name registers once: a duplicate is rejected and the first handler stays (SimQueryHost::Register).
	using MirrorQueryHandler = std::function<Json::Value(const Json::Value& args)>;
	class IMirrorQueryRegistrar
	{
	public:
		virtual ~IMirrorQueryRegistrar() = default;
		virtual bool Register(const char* name, MirrorQueryHandler handler) = 0;	// query; false if rejected (duplicate / full)
		virtual bool RegisterCommand(const char* name, MirrorQueryHandler handler) = 0;	// command; false if rejected
		virtual void Unregister(const char* name) = 0;
	};

	// Registers cells.layer.list, cells.layer.values, cells.select.get and cells.select.stats; `queries` must outlive the registration.
	// Returns false (and unregisters any it had registered) if the registrar rejects one.
	bool RegisterMirrorQueries(IMirrorQueryRegistrar& registrar, const MirrorQueries& queries);
	void UnregisterMirrorQueries(IMirrorQueryRegistrar& registrar);
}

#endif // DIA_DEBUG
