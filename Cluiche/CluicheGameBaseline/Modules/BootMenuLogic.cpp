#include "Modules/BootMenuLogic.h"

namespace Cluiche { namespace AppFlow {

void BootMenuLogic::SetStages(const Dia::Core::StringCRC* stages, unsigned int count)
{
    mCount = (count > kMaxStages) ? kMaxStages : count;
    for (unsigned int i = 0; i < kMaxStages; ++i)
        mVisited[i] = false;
    for (unsigned int i = 0; i < mCount; ++i)
        mStages[i] = stages[i];
}

void BootMenuLogic::SetProvider(const StageStatusModule* provider) { mProvider = provider; }

bool BootMenuLogic::HasStatus() const { return mProvider != nullptr; }

bool BootMenuLogic::GetBadge(const Dia::Core::StringCRC& stage, Badge& outBadge) const
{
    if (mProvider == nullptr)
        return false;
    const StageStatusModule::StageStatus* status = mProvider->Get(stage);
    if (status == nullptr)
        return false;
    outBadge.label = status->label;
    outBadge.severity = status->severity;
    return true;
}

BootMenuLogic::Counts BootMenuLogic::CountBySeverity() const
{
    Counts c;
    if (mProvider == nullptr)
        return c;
    for (unsigned int i = 0; i < mCount; ++i)
    {
        const StageStatusModule::StageStatus* status = mProvider->Get(mStages[i]);
        if (status != nullptr && status->severity == StageStatusModule::Severity::kOk)
            ++c.passed;
        else if (status != nullptr && status->severity == StageStatusModule::Severity::kFail)
            ++c.failed;
        else
            ++c.pending;
    }
    return c;
}

void BootMenuLogic::RestoreVisited()
{
    if (mProvider == nullptr)
        return;
    for (unsigned int i = 0; i < mCount; ++i)
    {
        const StageStatusModule::StageStatus* status = mProvider->Get(mStages[i]);
        if (status != nullptr && status->visited)
            mVisited[i] = true;
    }
}

void BootMenuLogic::MarkVisited(const Dia::Core::StringCRC& stage)
{
    for (unsigned int i = 0; i < mCount; ++i)
    {
        if (mStages[i] == stage)
        {
            mVisited[i] = true;
            return;
        }
    }
}

bool BootMenuLogic::IsVisited(const Dia::Core::StringCRC& stage) const
{
    for (unsigned int i = 0; i < mCount; ++i)
    {
        if (mStages[i] == stage)
            return mVisited[i];
    }
    return false;
}

const char* BootMenuLogic::ResolveTitle(const char* configValue)
{
    return (configValue != nullptr && configValue[0] != 0) ? configValue : kDefaultTitle;
}

const char* BootMenuLogic::ResolveProject(const char* configValue)
{
    return (configValue != nullptr && configValue[0] != 0) ? configValue : "";
}

} } // namespace Cluiche::AppFlow
