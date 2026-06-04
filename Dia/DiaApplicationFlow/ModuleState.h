#pragma once

namespace Dia { namespace ApplicationFlow {

// Lifecycle states a Module passes through.
// Defined here so headers that only need the state enum (e.g. LifecycleEvent.h)
// don't have to include the full Module.h.
enum class ModuleState
{
    kInactive,
    kStarting,
    kActive,
    kStopping,
    kFailed,
};

}} // namespace Dia::ApplicationFlow
