#pragma once

#include <DiaCore/Architecture/Singleton/Singleton.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <mutex>

namespace CluicheTest {

struct StageResult
{
    enum class State { kNotRun, kRunning, kPassed, kFailed, kTimeout };

    static constexpr unsigned int kMaxCheckpoints = 8;

    Dia::Core::StringCRC name;
    State state = State::kNotRun;
    unsigned int settleFrame = 0;
    unsigned int budgetFrames = 0;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxCheckpoints> checkpoints;
};

class TestResultsRegistry : public Dia::Core::Singleton<TestResultsRegistry>
{
public:
    void SetRunning(const Dia::Core::StringCRC& stageName, unsigned int budgetFrames);
    void SetRunning(const Dia::Core::StringCRC& stageName,
                    unsigned int budgetFrames,
                    const Dia::Core::StringCRC* checkpointNames,
                    unsigned int checkpointCount);
    void SetPassed(const Dia::Core::StringCRC& stageName, unsigned int frame);
    void SetFailed(const Dia::Core::StringCRC& stageName, unsigned int frame);
    void SetTimeout(const Dia::Core::StringCRC& stageName);

    const StageResult* GetResult(const Dia::Core::StringCRC& stageName) const;
    unsigned int GetResultCount() const;
    const StageResult& GetResultAt(unsigned int index) const;

    // Live state for HUD
    const Dia::Core::StringCRC& GetActiveStage() const;
    unsigned int GetActiveFrameCount() const;
    void SetActiveFrameCount(unsigned int frame);

    static void SetAutomationMode(bool value);
    static bool IsAutomationMode();

private:
    StageResult* FindOrCreate(const Dia::Core::StringCRC& stageName);
    StageResult* Find(const Dia::Core::StringCRC& stageName);
    const StageResult* Find(const Dia::Core::StringCRC& stageName) const;

    static constexpr unsigned int kMaxStages = 16;
    Dia::Core::Containers::DynamicArrayC<StageResult, kMaxStages> mResults;
    Dia::Core::StringCRC mActiveStage;
    unsigned int mActiveFrame = 0;
    mutable std::mutex mMutex;
};

} // namespace CluicheTest
