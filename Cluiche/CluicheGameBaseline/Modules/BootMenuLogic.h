#pragma once
#include <Modules/StageStatusModule.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

// Pure (no ImGui, no module machinery) logic behind BootMenuModule: stage list, visited
// bookkeeping, status lookup and severity counts. The provider may be null.
class BootMenuLogic {
public:
    static constexpr unsigned int kMaxStages = 64;
    static constexpr const char* kDefaultTitle = "Boot Menu";

    struct Counts {
        unsigned int passed = 0;
        unsigned int failed = 0;
        unsigned int pending = 0;
    };

    struct Badge {
        const char* label = nullptr;
        StageStatusModule::Severity severity = StageStatusModule::Severity::kNone;
    };

    // Replaces the stage list and clears visited markers.
    void SetStages(const Dia::Core::StringCRC* stages, unsigned int count);
    // Provider is optional; null means no badges and no counts.
    void SetProvider(const StageStatusModule* provider);

    bool HasStatus() const;
    // Fills badge and returns true only if a provider exists and has an entry for the stage.
    bool GetBadge(const Dia::Core::StringCRC& stage, Badge& outBadge) const;
    // Passed = kOk, Failed = kFail, Pending = everything else (incl. no entry). Zeros if no provider.
    Counts CountBySeverity() const;

    // Marks every listed stage visited whose provider entry has visited == true.
    void RestoreVisited();
    // Marks a listed stage visited (called when the nav request is sent).
    void MarkVisited(const Dia::Core::StringCRC& stage);
    bool IsVisited(const Dia::Core::StringCRC& stage) const;

    // Null or empty config value -> default title / empty project line.
    static const char* ResolveTitle(const char* configValue);
    static const char* ResolveProject(const char* configValue);

private:
    Dia::Core::StringCRC mStages[kMaxStages];
    bool mVisited[kMaxStages] = {};
    unsigned int mCount = 0;
    const StageStatusModule* mProvider = nullptr;
};

} } // namespace Cluiche::AppFlow
