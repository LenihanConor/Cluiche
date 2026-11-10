#pragma once

#include <DiaCore/Json/external/json/json.h>

#include <string>
#include <vector>

namespace Cluiche { namespace AppFlow {

// Pure helpers for the debug panel's domain-state push (Main PU). The panel root is {"<displayName>": {...}, ..., "simFrame": N}.
// Comparing the freshly parsed root with the previous one lets the panel receive only the domains whose state actually changed
// (a partial push), instead of re-serialising and re-parsing every domain every Sim frame.
struct DomainRootDelta
{
    bool                     structureChanged = false;   // a domain appeared/disappeared, or a root is not an object: push everything
    std::vector<std::string> changed;                    // domains present in both roots whose value differs ("simFrame" ignored)
};

DomainRootDelta ComputeDomainRootDelta(const Json::Value& prev, const Json::Value& next);

// {"__partial": true, "<name>": next[name], ...}. debug-panel.html merges the named domains into the state it already holds.
Json::Value BuildPartialRoot(const Json::Value& next, const std::vector<std::string>& names);

} } // namespace Cluiche::AppFlow
