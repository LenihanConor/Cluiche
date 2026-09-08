#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

// One panel command travelling MainPU -> SimPU.
//
// DiaDebugPanel runs on the Main PU (Ultralight / UIModule) while the debug
// domains and their drawers live on the Sim PU (VisualDebuggerModule).
// ModuleRef only resolves siblings inside the same ProcessingUnit, so the
// panel cannot call VisualDebuggerModule::EnqueueCommand directly. Commands
// therefore travel over the "DebugPanelCommand" EventStream, mirroring the
// existing BootMenuNavRequest / HUDNavRequest RenderPU -> SimPU pattern.
//
// argsJson carries the command payload verbatim; VisualDebuggerModule parses
// it on the Sim thread and hands the Json::Value to IDebugDomain::OnCommand.
struct DebugPanelCommandEvent
{
    static const int kMaxArgsJson = 512;

    Dia::Core::StringCRC domainId;
    Dia::Core::StringCRC cmd;
    char                 argsJson[kMaxArgsJson] = {};
};

} } // namespace Cluiche::AppFlow
