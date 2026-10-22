#pragma once

// ValueSchemaQueryHandler: the `valueschema.get {prefix?}` DiaDebugServer query (DiaValueSchema Feature 3, AC 8).
//
// Returns { "version": N, "entries": [ ... ] }. `entries` are the published entries whose FULL key ("<for>/<key>")
// starts with `prefix` (all when `prefix` is omitted or empty), in registration order; `version` is
// ValueSchemaService::Version() (changes whenever a new snapshot is published). Entry JSON uses the sidecar field
// names (for, key, kind, label, description, hint, unit, type, min, max, default, affects, related, source, values,
// unique, required, ref, keyRef, valueType, ui, history, payload, vocabulary); absent optional fields are omitted.
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
