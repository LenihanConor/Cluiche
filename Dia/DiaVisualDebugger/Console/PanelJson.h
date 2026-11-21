////////////////////////////////////////////////////////////////////////////////
// Filename: PanelJson.h
// Description: Builds the per-frame `updateDomainState` payload the debug panel
//              page receives: one object per domain, keyed by the domain's
//              DISPLAY NAME (the page keys its cards by it), with _displayName,
//              _group, _description and _accent added to the domain's own state.
//              Pure logic, extracted from DebugPanelPageModule so the exact JSON
//              can be pinned by golden-file contract tests.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
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
			// One domain's panel state: GetJSONState plus schemaVersion and the _id/_displayName/_group/
			// _description/_accent keys. Returns false (state still filled) when the display name is null.
			// BuildDomainStatesRoot is exactly this per domain, keyed by display name.
			bool BuildDomainState(IDebugDomain& domain, Json::Value& state, const ISchemaKeyResolver* resolver = nullptr);

			// Fills root (object) with one entry per domain, keyed by GetDisplayName().
			// Domains with a null display name are skipped.
			//
			// resolver (optional): when non-null and IsAvailable(), every domain state that has a
			// "controls" array and no "schemaVersion" of its own gets schemaVersion = resolver->Version().
			// The page (vsScan) reads schemaVersion from a domain state, not from the root.
			void BuildDomainStatesRoot(const DiaDebugDomainRegistry& registry, Json::Value& root,
									   const ISchemaKeyResolver* resolver = nullptr);
		}
	}
}

#endif // DIA_DEBUG
