// S-1 Reachability Validator: runs injected guarantees against a generated world.
// Validate() builds one NavigationQuery over the injected profiles, then runs every guarantee independently in
// injection order (never stops at the first failure) and aggregates. An empty profile list fails the overall result
// with a validation-level diagnostic and logs an error; guarantees still run (those that need navigation fail).
// Blocking and deterministic; no timeout of its own (the retry/timeout features wrap the call).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md (Validation Flow)
#pragma once

#include "ValidationTypes.h"

namespace Draoi {
namespace HexWorld {

class ReachabilityValidator
{
public:
    static ValidationResult Validate(const WorldStructure& world, const ValidationParams& params);
};

} // namespace HexWorld
} // namespace Draoi
