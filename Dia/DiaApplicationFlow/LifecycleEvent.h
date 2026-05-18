#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <DiaApplicationFlow/Module.h>

namespace Dia { namespace ApplicationFlow {

// LifecycleEvent — discrete application/module lifecycle notification emitted
// on the framework-owned $lifecycle EventStream.
//
// The event is a tagged union discriminated by `kind`.  Only the fields
// relevant to the active kind carry meaningful values.
//
// Reserved stream: StringCRC("$lifecycle")
// Reserved sender prefix: "$" (framework-owned; user $ prefix forbidden).

enum class LifecycleEventKind : unsigned int
{
    kStageTransitionRequested  = 0,
    kStageTransitionStarted    = 1,
    kStageTransitionCommitted  = 2,
    kRollbackAttempted         = 3,
    kShutdownRequested         = 4,
    kModuleStateChanged        = 5,
};

struct LifecycleEvent
{
    LifecycleEventKind kind = LifecycleEventKind::kStageTransitionRequested;

    // Stage transition / rollback fields (kinds 0-3)
    Dia::Core::StringCRC fromStage;
    Dia::Core::StringCRC toStage;
    unsigned int         rollbackAttempt = 0;  // kRollbackAttempted only

    // Module state change fields (kind 5)
    Dia::Core::StringCRC moduleInstanceId;
    ModuleState          previousState = ModuleState::kInactive;
    ModuleState          newState      = ModuleState::kInactive;
};

// Reserved stream and sender IDs — do NOT declare these in user manifests.
namespace Reserved {
    // The $lifecycle stream is auto-created by Application::Start(); modules
    // may list "$lifecycle" in their reads[] without a manifest declaration.
    inline Dia::Core::StringCRC LifecycleStreamId() { return Dia::Core::StringCRC("$lifecycle"); }
    inline Dia::Core::StringCRC FrameworkSenderId() { return Dia::Core::StringCRC("$framework"); }
} // namespace Reserved

}} // namespace Dia::ApplicationFlow
