////////////////////////////////////////////////////////////////////////////////
// Filename: DomainStateQuery.h
// Description: The `debug.domain_state` DebugServer query. Returns the same per-domain
//              panel state JSON the debug panel receives (IDebugDomain::GetJSONState plus
//              the _id/_displayName/_group/_description/_accent keys; see PanelJson.h).
//
//                {"id": "<domain id>"}  -> that domain's state object
//                {} / no args           -> the whole BuildDomainStatesRoot object (keyed by display name)
//                unknown / non-string id-> {"error": "..."}; nothing is queried or changed
//
// Threading: GetJSONState is called on the caller's thread. The host registers this on the
// DebugServer QueryRegistry from the Main PU module that already builds the panel payload
// (DebugPanelPageModule), and the DebugServer dispatches queries on that same Main thread.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/ISchemaKeyResolver.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			static constexpr const char* kDomainStateQueryName = "debug.domain_state";

			Json::Value QueryDomainState(const DiaDebugDomainRegistry& registry, const ISchemaKeyResolver* resolver,
										 const Json::Value& args);
		}
	}
}

#endif // DIA_DEBUG
