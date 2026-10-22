#pragma once

// ValueSchemaQueryHandler: the `valueschema.get {prefix?, keys?}` DiaDebugServer query (DiaValueSchema Feature 3 AC 8; Feature 6 AC 10).
//
// Returns { "version": N, "entries": [ ... ], "ramps": [ ... ] }. `entries` are the published entries whose FULL key ("<for>/<key>")
// starts with `prefix` (all when `prefix` is omitted or empty), in registration order; `version` is
// ValueSchemaService::Version() (changes whenever a new snapshot is published). Entry JSON uses the sidecar field
// names plus "origin" (see OriginOf; the same entry format as the export document, via ValueInfoToExportJson) (for, key, kind, label, description, hint, unit, type, min, max, default, affects, related, source, values,
// unique, required, ref, keyRef, valueType, ui, history, payload, vocabulary); absent optional fields are omitted.
//
// `keys` (Feature 6): a JSON array of exact full keys ("<for>/<key>"). When present as an array it WINS over `prefix`
// (prefix is ignored). Result order follows `keys`; duplicates appear once; unknown keys and non-string elements are
// skipped silently. An empty array returns zero entries (an explicit empty filter, not "all"). A `keys` that is not an
// array is ignored (one Debug log line) and `prefix`/all applies. valueschema.query_unresolved counts a query only when
// it matched nothing despite asking for something (always for prefix; for keys only when keys was non-empty).
//
// `ramps` (Feature 5, AC 12) is ALWAYS the full ramp set in registration order, regardless of `prefix` (ramps are
// global and small). Shape = the dia.ramps file format: { id, kind:"continuous", stops:[{t, colour}] } or
// { id, kind:"categorical", colours:[...] }; colours are lower-case "#rrggbb". Empty array when there are no ramps.
//
// The handler reads the service snapshot and holds no state of its own beyond two Counter pointers. It is a plain
// class so it is testable without a DebugServer; ValueSchemaModule registers it with the DebugServer QueryRegistry.
// Observation: trace zone + profile scope "ValueSchema.Query"; Debug log (channel VSC) of prefix and match count;
// counters valueschema.queries_served (every query) and valueschema.query_unresolved (queries with zero matches).
//
// Threading: Handle() is const over an immutable snapshot, so it is safe on the DebugServer thread. The referenced
// service must outlive the handler (the module destroys the handler before it replaces the service).

#include <DiaValueSchema/ValueSchemaService.h>
#include <json/json.h>

namespace Dia::Observation::Metric { class Counter; }

namespace Dia::ValueSchema
{
	// One entry in sidecar field names. Public so the Feature 6 export can reuse it.
	Json::Value ValueInfoToJson(const ValueInfo& info);

	// Where an entry came from: "sidecar:<path>" (backslashes normalised to "/") or, for entries whose source file is a
	// "code:" marker, "code:metric" | "code:command" | "code:arg" | "code:component_field" by ValueKind. Shared by the
	// Feature 6 export and the live query.
	Dia::Core::Containers::String512 OriginOf(const ValueInfo& info);

	// ValueInfoToJson plus "origin" (export document entry; also what valueschema.get returns per entry).
	Json::Value ValueInfoToExportJson(const ValueInfo& info);

	// One ramp in dia.ramps file format.
	Json::Value RampToJson(const RampData& ramp);

	class ValueSchemaQueryHandler
	{
	public:
		static constexpr const char* kQueryName = "valueschema.get";

		explicit ValueSchemaQueryHandler(const ValueSchemaService& service);

		Json::Value Handle(const Json::Value& args) const;

	private:
		const ValueSchemaService&			mService;
		Dia::Observation::Metric::Counter*	mQueriesServed = nullptr;
		Dia::Observation::Metric::Counter*	mQueryUnresolved = nullptr;
	};
}
