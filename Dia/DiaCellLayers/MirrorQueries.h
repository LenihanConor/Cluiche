#pragma once
#ifdef DIA_DEBUG

// MirrorQueries: the pull side of the editor mirror (Feature 5). Plain logic class, separate from DebugServer registration:
// RegisterMirrorQueries(QueryRegistry&, MirrorQueries&) wires it (host wiring calls it). Read-only: never mutates the viewer,
// registry or selection.
//
// Queries answered here:
//   cells.layer.list  (no args) -> { layersVersion, grids:[{id:"grid.<i>", index, width, height, layers:[{id, schemaKey, group,
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
// Threading: handlers run inline on the DebugServer's thread; the viewer lives on SimPU, so every read is taken under the
// viewer's state mutex (CellLayerViewerDomain::StateMutex). Borrowed references must outlive the object.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/LayerReader.h>

namespace Dia::DebugServer { class QueryRegistry; }

namespace Dia::CellLayers
{
	// Shared grid id format: "grid.<index>" (index = position in CellLayerRegistry::GridAt). Used by the view-state topic and the queries.
	namespace MirrorGridId
	{
		constexpr uint32_t kBufferSize = 24;
		uint32_t	IndexOf(const CellLayerRegistry& registry, const Dia::Graphics::CellGridDesc* grid);	// 0 when not found
		void		Format(uint32_t index, char (&out)[kBufferSize]);
	}

	class MirrorQueries
	{
	public:
		static const uint32_t kMaxSelectionChunk = 8192;
		static const uint32_t kMaxValuesChunk = 196608;		// cells per cells.layer.values call: 786,432 bytes -> 1 MiB of base64

		// schema may be null (no hotkeys). Registry, viewer and schema are borrowed.
		MirrorQueries(const CellLayerRegistry& registry, const CellLayerViewerDomain& viewer, const Dia::ValueSchema::ValueSchemaService* schema);

		Json::Value	LayerList() const;
		Json::Value	SelectionGet(const Json::Value& args) const;
		Json::Value	LayerValues(const Json::Value& args) const;

		// Test seam: lowers the per-call cell limit (clamped to [1, kMaxValuesChunk]) so chunking is testable on small grids.
		void		SetMaxValuesChunk(uint32_t cells);
		uint32_t	MaxValuesChunk() const { return mMaxValuesChunk; }

	private:
		const CellLayerRegistry&		mRegistry;
		const CellLayerViewerDomain&	mViewer;
		const Dia::ValueSchema::ValueSchemaService*	mSchema;
		uint32_t					mMaxValuesChunk = kMaxValuesChunk;
		mutable LayerReader			mReader;	// used only under the viewer's state mutex
	};

	// Registers cells.layer.list, cells.layer.values and cells.select.get; `queries` must outlive the registration.
	void RegisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry, const MirrorQueries& queries);
	void UnregisterMirrorQueries(Dia::DebugServer::QueryRegistry& registry);
}

#endif // DIA_DEBUG
