#include "Modules/BootMenuLogic.h"

#include <cstdio>
#include <cstring>

namespace Cluiche { namespace AppFlow {

namespace {

using Dia::Game::StageKind;

char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

int CompareNoCase(const char* a, const char* b)
{
    while (*a != 0 && *b != 0)
    {
        const char ca = Lower(*a);
        const char cb = Lower(*b);
        if (ca != cb)
            return ca < cb ? -1 : 1;
        ++a;
        ++b;
    }
    if (*a == *b) return 0;
    return *a == 0 ? -1 : 1;
}

bool ContainsNoCase(const char* haystack, const char* needle)
{
    if (needle[0] == 0)
        return true;
    for (const char* h = haystack; *h != 0; ++h)
    {
        const char* a = h;
        const char* b = needle;
        while (*a != 0 && *b != 0 && Lower(*a) == Lower(*b))
        {
            ++a;
            ++b;
        }
        if (*b == 0)
            return true;
    }
    return false;
}

void CopyText(char* dst, unsigned int cap, const char* src)
{
    if (src == nullptr)
        src = "";
    std::strncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

const char* GroupTitle(unsigned int g)
{
    static const char* const kNames[kStageGroupCount] = { "Showcase", "Tools", "Tests", "Other" };
    return kNames[g];
}

} // namespace

bool BootMenuLogic::EntryInGroup(const StageCatalogueSnapshotEntry& e, StageGroup g)
{
    switch (g)
    {
    case StageGroup::kShowcase: return Dia::Game::MaskHasKind(e.kinds, StageKind::kShowcase);
    case StageGroup::kTool:     return Dia::Game::MaskHasKind(e.kinds, StageKind::kTool);
    case StageGroup::kTest:     return Dia::Game::MaskHasKind(e.kinds, StageKind::kTest);
    case StageGroup::kOther:    return (e.kinds & 0x7) == 0;
    }
    return false;
}

bool BootMenuLogic::Matches(const StageCatalogueSnapshotEntry& e) const
{
    if (mFallback)
        return true;
    for (unsigned int c = 0; c < mActiveChipCount; ++c)
    {
        bool found = false;
        for (unsigned int t = 0; t < e.tagCount && t < StageCatalogueSnapshotEntry::kMaxTags && !found; ++t)
            found = std::strcmp(e.tags[t], mActiveChips[c]) == 0;
        if (!found)
            return false;
    }
    if (mSearch[0] != 0)
    {
        const char* name = e.name.AsChar();
        if (!ContainsNoCase(name != nullptr ? name : "", mSearch) && !ContainsNoCase(e.description, mSearch))
            return false;
    }
    return true;
}

int BootMenuLogic::FindEntry(const Dia::Core::StringCRC& stage) const
{
    for (unsigned int i = 0; i < mCount; ++i)
        if (mEntries[i].name == stage)
            return static_cast<int>(i);
    return -1;
}

void BootMenuLogic::Rebuild()
{
    for (unsigned int g = 0; g < kGroupCount; ++g)
        mGroupRowCount[g] = 0;
    for (unsigned int i = 0; i < mCount; ++i)
    {
        if (!Matches(mEntries[i]))
            continue;
        for (unsigned int g = 0; g < kGroupCount; ++g)
            if (EntryInGroup(mEntries[i], static_cast<StageGroup>(g)))
                mGroupRows[g][mGroupRowCount[g]++] = i;
    }
    mVisibleGroupCount = 0;
    for (unsigned int g = 0; g < kGroupCount; ++g)
        if (mGroupRowCount[g] > 0)
            mVisibleGroups[mVisibleGroupCount++] = g;
}

void BootMenuLogic::BuildChips()
{
    mChipCount = 0;
    // Kinds first, in group order, when present in the catalogue.
    for (unsigned int k = 0; k < Dia::Game::kStageKindCount; ++k)
    {
        bool present = false;
        for (unsigned int i = 0; i < mCount && !present; ++i)
            for (unsigned int t = 0; t < mEntries[i].tagCount && t < StageCatalogueSnapshotEntry::kMaxTags && !present; ++t)
                present = std::strcmp(mEntries[i].tags[t], Dia::Game::kStageKindNames[k]) == 0;
        if (present && mChipCount < kMaxChips)
            CopyText(mChips[mChipCount++], kChipChars, Dia::Game::kStageKindNames[k]);
    }
    const unsigned int firstOther = mChipCount;
    for (unsigned int i = 0; i < mCount; ++i)
    {
        for (unsigned int t = 0; t < mEntries[i].tagCount && t < StageCatalogueSnapshotEntry::kMaxTags; ++t)
        {
            const char* tag = mEntries[i].tags[t];
            if (tag[0] == 0)
                continue;
            bool dup = false;
            for (unsigned int c = 0; c < mChipCount && !dup; ++c)
                dup = std::strcmp(mChips[c], tag) == 0;
            if (dup || mChipCount >= kMaxChips)
                continue;
            // Insertion into the alphabetical tail (case-insensitive, then case-sensitive tie-break).
            unsigned int pos = mChipCount;
            while (pos > firstOther)
            {
                int cmp = CompareNoCase(mChips[pos - 1], tag);
                if (cmp == 0)
                    cmp = std::strcmp(mChips[pos - 1], tag);
                if (cmp <= 0)
                    break;
                std::memcpy(mChips[pos], mChips[pos - 1], kChipChars);
                --pos;
            }
            CopyText(mChips[pos], kChipChars, tag);
            ++mChipCount;
        }
    }
}

void BootMenuLogic::SetSnapshot(const StageCatalogueSnapshot& snapshot)
{
    mCount = snapshot.entryCount > kMaxStages ? kMaxStages : snapshot.entryCount;
    for (unsigned int i = 0; i < mCount; ++i)
        mEntries[i] = snapshot.entries[i];
    mRevision = snapshot.revision;
    mHasSnapshot = true;
    mFallback = false;
    BuildChips();
    Rebuild();
}

void BootMenuLogic::SetFallbackStages(const Dia::Core::StringCRC* stages, unsigned int count)
{
    mCount = count > kMaxStages ? kMaxStages : count;
    for (unsigned int i = 0; i < mCount; ++i)
    {
        mEntries[i] = StageCatalogueSnapshotEntry();
        mEntries[i].name = stages[i];
    }
    mHasSnapshot = false;
    mFallback = true;
    mChipCount = 0;
    Rebuild();
}

bool BootMenuLogic::GetGroup(unsigned int groupIndex, GroupInfo& outGroup) const
{
    if (groupIndex >= mVisibleGroupCount)
        return false;
    const unsigned int g = mVisibleGroups[groupIndex];
    outGroup = GroupInfo();
    outGroup.group = static_cast<StageGroup>(g);
    outGroup.stageCount = mGroupRowCount[g];
    outGroup.collapsed = mCollapsed[g];
    if (mFallback)
    {
        CopyText(outGroup.title, kMaxTitle, kFallbackHeading);
    }
    else if (outGroup.group == StageGroup::kTest)
    {
        const Counts c = TestRollup();
        std::snprintf(outGroup.title, kMaxTitle, "Tests (%u passed, %u failed, %u pending)", c.passed, c.failed, c.pending);
    }
    else
    {
        std::snprintf(outGroup.title, kMaxTitle, "%s (%u)", GroupTitle(g), outGroup.stageCount);
    }
    return true;
}

unsigned int BootMenuLogic::RowCount(unsigned int groupIndex) const
{
    if (groupIndex >= mVisibleGroupCount)
        return 0;
    const unsigned int g = mVisibleGroups[groupIndex];
    unsigned int n = 0;
    for (unsigned int r = 0; r < mGroupRowCount[g]; ++r)
        n += 1 + mEntries[mGroupRows[g][r]].presetCount;
    return n;
}

bool BootMenuLogic::GetRow(unsigned int groupIndex, unsigned int rowIndex, Row& outRow) const
{
    if (groupIndex >= mVisibleGroupCount)
        return false;
    const unsigned int g = mVisibleGroups[groupIndex];
    unsigned int row = 0;
    for (unsigned int r = 0; r < mGroupRowCount[g]; ++r)
    {
        const StageCatalogueSnapshotEntry& e = mEntries[mGroupRows[g][r]];
        if (row == rowIndex)
        {
            outRow = Row();
            outRow.stage = e.name;
            outRow.name = e.name.AsChar() != nullptr ? e.name.AsChar() : "";
            outRow.description = e.description;
            outRow.kinds = e.kinds;
            return true;
        }
        ++row;
        const unsigned int presets = e.presetCount > StageCatalogueSnapshotEntry::kMaxPresets
                                         ? StageCatalogueSnapshotEntry::kMaxPresets
                                         : e.presetCount;
        for (unsigned int p = 0; p < presets; ++p, ++row)
        {
            if (row == rowIndex)
            {
                outRow = Row();
                outRow.stage = e.name;
                outRow.preset = Dia::Core::StringCRC(e.presets[p].name);
                outRow.isPreset = true;
                outRow.name = e.presets[p].name;
                outRow.description = e.presets[p].description;
                outRow.kinds = e.kinds;
                return true;
            }
        }
    }
    return false;
}

unsigned int BootMenuLogic::VisitedCount() const
{
    unsigned int n = 0;
    for (unsigned int i = 0; i < mCount; ++i)
        if (IsVisited(mEntries[i].name))
            ++n;
    return n;
}

BootMenuLogic::Counts BootMenuLogic::TestRollup() const
{
    Counts c;
    if (mFallback)
        return c;
    for (unsigned int i = 0; i < mCount; ++i)
    {
        if (!Dia::Game::MaskHasKind(mEntries[i].kinds, StageKind::kTest))
            continue;
        switch (mEntries[i].severity)
        {
        case StageResultSeverity::kOk:
        case StageResultSeverity::kWarn: ++c.passed; break;
        case StageResultSeverity::kFail: ++c.failed; break;
        default:                         ++c.pending; break;   // none / missing / visited with no result
        }
    }
    return c;
}

bool BootMenuLogic::GetBadge(const Dia::Core::StringCRC& stage, Badge& outBadge) const
{
    if (mFallback)
        return false;
    const int idx = FindEntry(stage);
    if (idx < 0 || !Dia::Game::MaskHasKind(mEntries[idx].kinds, StageKind::kTest))
        return false;
    const StageCatalogueSnapshotEntry& e = mEntries[idx];
    outBadge.severity = e.severity;
    switch (e.severity)
    {
    case StageResultSeverity::kOk:
        outBadge.colour = BadgeColour::kGreen;
        outBadge.label = e.label[0] != 0 ? e.label : "PASSED";
        break;
    case StageResultSeverity::kWarn:
        outBadge.colour = BadgeColour::kAmber;
        outBadge.label = e.label[0] != 0 ? e.label : "WARN";
        break;
    case StageResultSeverity::kFail:
        outBadge.colour = BadgeColour::kRed;
        outBadge.label = e.label[0] != 0 ? e.label : "FAILED";
        break;
    default:
        outBadge.colour = BadgeColour::kGrey;
        outBadge.label = "PENDING";
        break;
    }
    return true;
}

unsigned int BootMenuLogic::BadgeColourRgba(BadgeColour colour)
{
    switch (colour)
    {
    case BadgeColour::kGreen: return 0x4CAF50FFu;
    case BadgeColour::kAmber: return 0xFFB300FFu;
    case BadgeColour::kRed:   return 0xE53935FFu;
    default:                  return 0x9E9E9EFFu;
    }
}

const char* BootMenuLogic::ChipName(unsigned int index) const
{
    return index < ChipCount() ? mChips[index] : "";
}

bool BootMenuLogic::IsChipActive(unsigned int index) const
{
    if (index >= ChipCount())
        return false;
    for (unsigned int a = 0; a < mActiveChipCount; ++a)
        if (std::strcmp(mActiveChips[a], mChips[index]) == 0)
            return true;
    return false;
}

void BootMenuLogic::SetChipActive(unsigned int index, bool active)
{
    if (index >= ChipCount())
        return;
    const bool current = IsChipActive(index);
    if (current == active)
        return;
    if (active)
    {
        if (mActiveChipCount >= kMaxActiveChips)
            return;
        CopyText(mActiveChips[mActiveChipCount++], kChipChars, mChips[index]);
    }
    else
    {
        unsigned int kept = 0;
        for (unsigned int a = 0; a < mActiveChipCount; ++a)
            if (std::strcmp(mActiveChips[a], mChips[index]) != 0)
                std::memcpy(mActiveChips[kept++], mActiveChips[a], kChipChars);
        mActiveChipCount = kept;
    }
    Rebuild();
}

void BootMenuLogic::ClearFilters()
{
    mActiveChipCount = 0;
    mSearch[0] = 0;
    Rebuild();
}

void BootMenuLogic::SetSearch(const char* text)
{
    CopyText(mSearch, kMaxSearch, text);
    Rebuild();
}

bool BootMenuLogic::HasActiveFilters() const
{
    return mActiveChipCount > 0 || mSearch[0] != 0;
}

void BootMenuLogic::MarkVisited(const Dia::Core::StringCRC& stage)
{
    if (IsVisited(stage) || mVisitedCount >= kMaxVisited)
        return;
    mVisited[mVisitedCount++] = stage;
}

bool BootMenuLogic::IsVisited(const Dia::Core::StringCRC& stage) const
{
    for (unsigned int i = 0; i < mVisitedCount; ++i)
        if (mVisited[i] == stage)
            return true;
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
