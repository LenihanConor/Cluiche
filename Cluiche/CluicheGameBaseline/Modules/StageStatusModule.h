#pragma once
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>

namespace Cluiche { namespace AppFlow {

// Generic per-stage status table (label + severity + visited). Knows nothing about tests.
class StageStatusModule : public Dia::ApplicationFlow::RenderModule {
public:
    enum class Severity { kNone, kOk, kWarn, kFail };

    struct StageStatus {
        const char* label = nullptr;
        Severity severity = Severity::kNone;
        bool visited = false;
    };

    static constexpr int kMaxStages = 64;

    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "Per-stage status table (label, severity, visited) keyed by stage CRC";
    explicit StageStatusModule(const Dia::Core::StringCRC& instanceId);

    // Stores or overwrites the status for a stage. Returns false only when the stage is new and the table is full.
    bool Set(const Dia::Core::StringCRC& stage, const StageStatus& status);
    // Returns the stored status, or nullptr if the stage is unknown.
    const StageStatus* Get(const Dia::Core::StringCRC& stage) const;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    struct Entry {
        Dia::Core::StringCRC stage;
        StageStatus status;
    };
    Entry mEntries[kMaxStages];
    int mCount = 0;
};

} } // namespace Cluiche::AppFlow
