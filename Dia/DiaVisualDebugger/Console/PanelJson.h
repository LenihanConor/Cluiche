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
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			// Fills root (object) with one entry per domain, keyed by GetDisplayName().
			// Domains with a null display name are skipped.
			void BuildDomainStatesRoot(const DiaDebugDomainRegistry& registry, Json::Value& root);
		}
	}
}

#endif // DIA_DEBUG
