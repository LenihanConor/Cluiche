#pragma once
#include "Modules/F2Harness/F2TestRunnerModule.h"   // kF2CheckCount
#include "Types/StageResult.h"
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace CoW {

// Name of the F2CoreRuntime stage as the StageCatalogue knows it.
constexpr const char* kF2StageName = "F2CoreRuntime";

// Decision behind the single StageResult event sent when an F-2 run completes:
// every check passed -> kOk "PASS"; anything else (any fail, or a check that never resolved) -> kFail "FAIL".
inline void BuildF2StageResult(uint32_t passCount, uint32_t failCount, uint64_t utcMs, Cluiche::AppFlow::StageResultEvent& out)
{
    const bool allPass = failCount == 0 && passCount == static_cast<uint32_t>(kF2CheckCount);
    out.stage = Dia::Core::StringCRC(kF2StageName);
    out.result.severity = allPass ? Cluiche::AppFlow::StageResultSeverity::kOk : Cluiche::AppFlow::StageResultSeverity::kFail;
    out.result.SetLabel(allPass ? "PASS" : "FAIL");
    out.result.utcMs = utcMs;
}

} // namespace CoW
