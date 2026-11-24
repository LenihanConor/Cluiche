#include "StageCatalogue.h"

namespace Cluiche { namespace AppFlow {

const char* StageGroupName(StageGroup group)
{
    switch (group)
    {
    case StageGroup::kShowcase: return "showcase";
    case StageGroup::kTool:     return "tool";
    case StageGroup::kTest:     return "test";
    default:                    return "other";
    }
}

void StageCatalogue::Clear() { mEntries.RemoveAll(); mOmittedCount = 0; mDroppedCount = 0; }

bool StageCatalogue::Add(const StageCatalogueEntry& entry)
{
    if (mEntries.IsFull()) { return false; }
    mEntries.Add(entry);
    return true;
}

int StageCatalogue::Find(const Dia::Core::StringCRC& name) const
{
    for (unsigned int i = 0; i < mEntries.Size(); ++i)
    {
        if (mEntries[i].name == name) { return static_cast<int>(i); }
    }
    return -1;
}

Dia::Game::StageKindMask StageCatalogue::Kinds(unsigned int entryIndex) const
{
    return entryIndex < mEntries.Size() ? mEntries[entryIndex].kinds : static_cast<Dia::Game::StageKindMask>(0);
}

void StageCatalogue::Filter(const Dia::Core::StringCRC* tags, unsigned int n, StageCatalogueIndices& out) const
{
    out.RemoveAll();
    for (unsigned int i = 0; i < mEntries.Size(); ++i)
    {
        bool all = true;
        for (unsigned int t = 0; t < n && all; ++t)
        {
            bool found = false;
            for (unsigned int k = 0; k < mEntries[i].tags.Size() && !found; ++k)
            {
                found = (mEntries[i].tags[k] == tags[t]);
            }
            all = found;
        }
        if (all) { out.Add(i); }
    }
}

void StageCatalogue::GroupByKind(StageCatalogueRows& out) const
{
    out.RemoveAll();
    for (unsigned int g = 0; g < kStageGroupCount; ++g)
    {
        const StageGroup group = static_cast<StageGroup>(g);
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            const Dia::Game::StageKindMask kinds = mEntries[i].kinds;
            const bool member = (group == StageGroup::kOther)
                ? (kinds == 0)
                : Dia::Game::MaskHasKind(kinds, static_cast<Dia::Game::StageKind>(g));
            if (member)
            {
                StageCatalogueRow row;
                row.group = group;
                row.entryIndex = i;
                out.Add(row);
            }
        }
    }
}

}} // namespace Cluiche::AppFlow
