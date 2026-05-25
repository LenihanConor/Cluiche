#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

void TestResultsRegistry::SetRunning(const Dia::Core::StringCRC& stageName, unsigned int budgetFrames)
{
    SetRunning(stageName, budgetFrames, nullptr, 0);
}

void TestResultsRegistry::SetRunning(const Dia::Core::StringCRC& stageName,
                                     unsigned int budgetFrames,
                                     const Dia::Core::StringCRC* checkpointNames,
                                     unsigned int checkpointCount)
{
    std::lock_guard<std::mutex> lock(mMutex);
    StageResult* r = FindOrCreate(stageName);
    r->state = StageResult::State::kRunning;
    r->budgetFrames = budgetFrames;
    r->settleFrame = 0;
    r->checkpoints.RemoveAll();
    for (unsigned int i = 0; i < checkpointCount && i < StageResult::kMaxCheckpoints; ++i)
        r->checkpoints.Add(checkpointNames[i]);
    mActiveStage = stageName;
    mActiveFrame = 0;
}

void TestResultsRegistry::SetPassed(const Dia::Core::StringCRC& stageName, unsigned int frame)
{
    std::lock_guard<std::mutex> lock(mMutex);
    StageResult* r = Find(stageName);
    if (r)
    {
        r->state = StageResult::State::kPassed;
        r->settleFrame = frame;
        DIA_LOG_INFO("CluicheTest", "Stage '%s' PASSED at frame %u", stageName.AsChar(), frame);
    }
}

void TestResultsRegistry::SetFailed(const Dia::Core::StringCRC& stageName, unsigned int frame)
{
    std::lock_guard<std::mutex> lock(mMutex);
    StageResult* r = Find(stageName);
    if (r)
    {
        r->state = StageResult::State::kFailed;
        r->settleFrame = frame;
    }
}

void TestResultsRegistry::SetTimeout(const Dia::Core::StringCRC& stageName)
{
    std::lock_guard<std::mutex> lock(mMutex);
    StageResult* r = Find(stageName);
    if (r)
    {
        r->state = StageResult::State::kTimeout;
        DIA_LOG_INFO("CluicheTest", "Stage '%s' TIMEOUT at frame budget", stageName.AsChar());
    }
}

const StageResult* TestResultsRegistry::GetResult(const Dia::Core::StringCRC& stageName) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return Find(stageName);
}

unsigned int TestResultsRegistry::GetResultCount() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mResults.Size();
}

const StageResult& TestResultsRegistry::GetResultAt(unsigned int index) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mResults[index];
}

const Dia::Core::StringCRC& TestResultsRegistry::GetActiveStage() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mActiveStage;
}

unsigned int TestResultsRegistry::GetActiveFrameCount() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mActiveFrame;
}

void TestResultsRegistry::SetActiveFrameCount(unsigned int frame)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mActiveFrame = frame;
}

StageResult* TestResultsRegistry::FindOrCreate(const Dia::Core::StringCRC& stageName)
{
    for (unsigned int i = 0; i < mResults.Size(); ++i)
    {
        if (mResults[i].name == stageName)
            return &mResults[i];
    }

    StageResult newResult;
    newResult.name = stageName;
    mResults.Add(newResult);
    return &mResults[mResults.Size() - 1];
}

StageResult* TestResultsRegistry::Find(const Dia::Core::StringCRC& stageName)
{
    for (unsigned int i = 0; i < mResults.Size(); ++i)
    {
        if (mResults[i].name == stageName)
            return &mResults[i];
    }
    return nullptr;
}

const StageResult* TestResultsRegistry::Find(const Dia::Core::StringCRC& stageName) const
{
    for (unsigned int i = 0; i < mResults.Size(); ++i)
    {
        if (mResults[i].name == stageName)
            return &mResults[i];
    }
    return nullptr;
}

} // namespace CluicheTest
