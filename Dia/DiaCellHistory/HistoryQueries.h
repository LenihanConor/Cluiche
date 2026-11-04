#pragma once
#ifdef DIA_DEBUG

// HistoryQueries: the catch-up and pull queries of DiaCellHistory Feature 5 (Export), sim-hosted like SnapshotCommands
// (IMirrorQueryRegistrar; handlers run on SimPU beside the recorder, no locks; Register/Unregister also on SimPU). The recorder (and
// the optional SnapshotManager / observation) are borrowed and must outlive the registration. Never registered into the DiaAPI
// CommandRegistry. Read-only: a query (or an error) changes nothing. Errors: { "error": "<reason>" }. Replies are built in one
// call on the recorder's PU; there is no state between calls (a cursor in `next` carries everything).
//
//   cells.history.meta        { part?, offset? }                       -> see Meta
//   cells.history.aggregates  { from?, to?, layers? }                  -> see Aggregates
//   cells.history.events      { from?, to?, types?, skip? }            -> see Events
//   cells.history.cells       { layer, from?, to?, cells?, cellOffset? } -> see Cells
//
// SIZE CAP (deviation from the export spec): the spec says each reply stays under 1 MB. A sim-hosted reply is limited to
// kSimPayloadBytes = 16,384 bytes of serialised JSON (Dia/DiaDebugServer/SimHostedTypes.h), so every reply of these queries is
// built to serialise to at most MaxReplyBytes() (default kMaxReplyBytes = 15,000, margin for the host envelope) and carries a
// `next` cursor when more remains. Consequence: a catch-up takes about 64 times more round trips than the spec assumed (Open
// Design Question 2). Test seam: SetMaxReplyBytes (clamped to [kMinReplyBytes, kMaxReplyBytes]).
//
// Conventions: `from` is inclusive, `to` is EXCLUSIVE; defaults are the whole retained window. Sample indices are the
// recorder's absolute indices. Arrays use the file's field names and HistoryEncoding base64 encodings.
//
// Meta. Default part "core": { part:"core", header:{app?,git_commit?,world,sampleCount,sampleFirst,aggregateFirst,cellFirst},
//   bands, groups:[{id,layer?,value?,label}], tracked, gaps:{aggregates,cells,events}, structure:"<16 hex>",
//   layers:[{id,kind,first,schemaKey?,values?}], snapshots:[{name,sample,label}], next? }. `structure` is the recorder's
//   StructureHash() (changes when a layer or grouping is added, never with new samples). groups then layers are filled in order
//   until the reply is full; `next` = { part:"groups"|"layers", offset } names the first omitted item and continues with
//   { part, groups:[...], layers:[...], next? } (the item arrays of a continuation are the same shape as in core). part "schema"
//   returns { part:"schema", entries:[...], ramps:[...], next?:{part:"schema",offset} } where offset runs over the entries then the
//   ramps of the file's embedded schema (ExportSubset of the same keys); concatenating the pages equals the file's `schema`.
// Aggregates. { structure, from, count, next?:{from}, samples:[{tick,label,band}], groupSizes:{group:b64 u16 per sample},
//   rows:{layer:{kind, first, values?, groups:{group:{min,mean,max}|b64 u16 sample-major}}} } for as many whole samples as fit
//   (at least 1). Arrays cover [from, from+count); a sample before the layer's `first` is NaN (floats) / zero counts (categories),
//   exactly as in the file. If one sample of the requested layers does not fit: error "reply too large: request fewer layers".
// Events. { from, to, events:[file-shaped], next?:{from,skip} } at most MaxEvents() (2,000) events and within the budget. `next`
//   resumes inside a sample: `from` is the sample and `skip` the number of that sample's events (before any `types` filter)
//   already delivered; pass both back (request fields `from`, `skip`). Keep the same `to` and `types` while following a cursor.
// Cells. For a TRACKED layer: { layer, kind, min?, max?, width, height, from, to, cells, cellOffset, cellCount, cellTotal, data,
//   next?:{from,cellOffset} }. `cells` is the optional request index list (at most kMaxCellList entries); the reply echoes the
//   slice returned, or null for all cells. The traversal is row-major over samples x cell columns: `data` holds one base64 string
//   per sample in [from,to) of the columns [cellOffset, cellOffset+cellCount) (uint16 codes for float layers, uint8 for category).
//   When one whole row (all columns) does not fit the reply holds ONE sample and a column slice, and next = {from, cellOffset+cellCount}
//   until the row is complete, then {from+1, 0}. A request cursor with cellOffset > 0 returns the rest of that one row.

#include <DiaCellLayers/MirrorQueries.h>

#include <stdint.h>

namespace Dia::CellHistory
{
	class CellHistoryRecorder;
	class SnapshotManager;
	class QueryObservation;

	class HistoryQueries
	{
	public:
		static constexpr uint32_t kNameCount = 4;
		static constexpr uint32_t kMaxReplyBytes = 15000;	// serialised JSON bytes per reply (the host limit is 16,384)
		static constexpr uint32_t kMinReplyBytes = 64;
		static constexpr uint32_t kMaxEventsPerReply = 2000;
		static constexpr uint32_t kMaxCellList = 2048;

		explicit HistoryQueries(const CellHistoryRecorder& recorder);

		// false (nothing left registered by this call) if the registrar rejects a name. Idempotent when already registered.
		bool Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar);
		void Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar);

		void SetAppName(const char* app);				// optional: header.app of meta
		void SetBuildInfo(const char* gitCommit);		// optional: header.git_commit of meta
		void SetSnapshots(const SnapshotManager* manager) { mSnapshots = manager; }
		void SetObservation(QueryObservation* obs) { mObs = obs; }

		Json::Value Meta(const Json::Value& args);
		Json::Value Aggregates(const Json::Value& args);
		Json::Value Events(const Json::Value& args);
		Json::Value Cells(const Json::Value& args);

		// Test seams: lower the per-reply byte budget (clamped to [kMinReplyBytes, kMaxReplyBytes]) and the event cap (clamped to [1, 2000]).
		void		SetMaxReplyBytes(uint32_t bytes);
		uint32_t	MaxReplyBytes() const { return mMaxReplyBytes; }
		void		SetMaxEvents(uint32_t events);
		uint32_t	MaxEvents() const { return mMaxEvents; }

	private:
		Json::Value Fail(const char* query, const char* message) const;
		Json::Value Finish(const char* query, int kind, Json::Value& reply, bool clamped);

		const CellHistoryRecorder&	mRecorder;
		const SnapshotManager*		mSnapshots = nullptr;
		QueryObservation*			mObs = nullptr;
		uint32_t					mMaxReplyBytes = kMaxReplyBytes;
		uint32_t					mMaxEvents = kMaxEventsPerReply;
		char						mApp[64] = {};
		char						mGit[64] = {};
		bool						mClampWarned = false;
		bool						mRegistered = false;
	};
}

#endif // DIA_DEBUG
