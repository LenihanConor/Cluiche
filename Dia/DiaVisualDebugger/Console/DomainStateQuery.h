////////////////////////////////////////////////////////////////////////////////
// Filename: DomainStateQuery.h
// Description: The `debug.domain_state` DebugServer query. Answers from the parsed domain
//              state snapshot (Console::DomainStateSnapshot, produced on SimPU), i.e. the same
//              per-domain panel state JSON the debug panel receives (IDebugDomain::GetJSONState
//              plus the _id/_displayName/_group/_description/_accent keys; see PanelJson.h).
//
//                {"id": "<domain id>"}  -> that domain's state object (matched on `_id`)
//                {} / no args           -> the whole root (keyed by display name, plus "simFrame")
//                unknown / non-string id-> {"error": "..."}
//                no snapshot yet        -> {"error": "no domain snapshot yet"}
//
// Threading: pure; touches no domain or registry. The host (DebugPanelPageModule, Main PU) owns
// the parsed root and registers this on the DebugServer QueryRegistry.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/DomainStateSnapshot.h>
#include <DiaVisualDebugger/Domain/ISchemaKeyResolver.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			static constexpr const char* kDomainStateQueryName = "debug.domain_state";

			// Parses snapshot.json into root: a JSON object keyed by display name, plus a numeric
			// "simFrame" member. Duplicate display names keep the last (as BuildDomainStatesRoot).
			// On failure (bad length, parse error, not an object) returns false and leaves root untouched.
			bool ParseDomainStateSnapshot(const DomainStateSnapshot& snapshot, Json::Value& root);

			// Main-side stamp, same rule as BuildDomainState: every domain object with a "controls" array and
			// no "schemaVersion" gets resolver->Version(). No-op for a null or unavailable resolver.
			void ApplySchemaVersion(Json::Value& root, const ISchemaKeyResolver* resolver);

			// root is the result of ParseDomainStateSnapshot, or null before the first snapshot arrived.
			Json::Value QueryDomainState(const Json::Value* root, const Json::Value& args);
		}
	}
}

#endif // DIA_DEBUG
