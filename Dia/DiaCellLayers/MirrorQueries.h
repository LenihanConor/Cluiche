#pragma once
#ifdef DIA_DEBUG

// MirrorQueries: the pull side of the editor mirror (Feature 5). Plain logic class, separate from DebugServer registration:
// RegisterMirrorQueries(QueryRegistry&, MirrorQueries&) wires it (host wiring calls it). Read-only: never mutates the viewer,
// registry or selection.
//
// Queries answered here:
//   cells.layer.list  (no args) -> { layersVersion, grids:[{id:"<grid id>"|"grid.<i>", index, width, height, layers:[{id, schemaKey, group,
//                       kind:"float"|"category", hotkey}]}], overlays:[{id, label, group}] }
//                       Same data source as the DiaAPI command of that name (CellLayerViewerDomain::ListLayers); this adds
//                       layersVersion, grid ids, schemaKey and hotkey. hotkey is "" when unbound or there is no schema.
//   cells.select.get  { offset?, count? } -> { version, hash (16 hex), total, offset, indices:[..] } of the active grid's
//                       selection, ascending, at most kMaxSelectionChunk per call (a larger count is clamped; Warning on CEL).
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
// Threading: handlers run inline on the DebugServer's thread; the viewer lives on SimPU, so every read is taken under the
// viewer's state mutex (CellLayerViewerDomain::StateMutex). Borrowed references must outlive the object.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerReader.h>

#include <stdint.h>
#include <vector>

namespace Dia::DebugServer { class QueryRegistry; }
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
		static const uint32_t kMaxSelectionChunk = 8192;
		static const uint32_t kStatsCacheCapacity = 8;	// stats cache entries per frame (round-robin eviction)
		static const uint32_t kMaxValuesChunk = 196608;		// cells per cells.layer.values call: 786,432 bytes -> 1 MiB of base64

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
		mutable LayerReader			mReader;	// used only under the viewer's state mutex

		// Stats cache (under the viewer's state mutex).
		uint32_t					mFrame = 0;
		struct StatsCacheEntry
		{
			bool					valid = false;
			uint32_t				frame = 0;
			std::vector<uint64_t>	key;	// variable length (layer set); the header already used std::vector for this
			Json::Value				result;
		};
		mutable StatsCacheEntry		mStatsCache[kStatsCacheCapacity];
		mutable uint32_t			mStatsCacheNext = 0;	// round-robin insert slot
		mutable uint32_t			mStatsComputeCount = 0;

		Dia::Observation::Metric::Counter*	mMetricQueries[4] = { nullptr, nullptr, nullptr, nullptr };	// by Kind
		Dia::Observation::Metric::Counter*	mMetricBytes = nullptr;
	};

	// Registers cells.layer.list, cells.layer.values, cells.select.get and cells.select.stats; `queries` must outlive the registration.
	void RegisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry, const MirrorQueries& queries);
	void UnregisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry);
}

#endif // DIA_DEBUG
