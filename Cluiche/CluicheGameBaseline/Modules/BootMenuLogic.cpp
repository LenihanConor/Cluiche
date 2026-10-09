#include "Modules/BootMenuLogic.h"

namespace Cluiche { namespace AppFlow {

// RED stubs: deliberately wrong so the tests fail on assertions.

void BootMenuLogic::SetStages(const Dia::Core::StringCRC*, unsigned int) {}
void BootMenuLogic::SetProvider(const StageStatusModule*) {}

bool BootMenuLogic::HasStatus() const { return true; }

bool BootMenuLogic::GetBadge(const Dia::Core::StringCRC&, Badge&) const { return false; }

BootMenuLogic::Counts BootMenuLogic::CountBySeverity() const
{
    Counts c;
    c.passed = 99; c.failed = 99; c.pending = 99;
    return c;
}

void BootMenuLogic::RestoreVisited() {}
void BootMenuLogic::MarkVisited(const Dia::Core::StringCRC&) {}
bool BootMenuLogic::IsVisited(const Dia::Core::StringCRC&) const { return false; }

const char* BootMenuLogic::ResolveTitle(const char*) { return ""; }
const char* BootMenuLogic::ResolveProject(const char*) { return "stub"; }

} } // namespace Cluiche::AppFlow
