#include <gtest/gtest.h>

#include <Modules/BootMenuLogic.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstring>
#include <memory>
#include <initializer_list>

using namespace Cluiche::AppFlow;
using Dia::Core::StringCRC;
using Dia::Game::KindBit;
using Dia::Game::StageKind;
using Sev = StageResultSeverity;

namespace {

const Dia::Game::StageKindMask kShow = KindBit(StageKind::kShowcase);
const Dia::Game::StageKindMask kTool = KindBit(StageKind::kTool);
const Dia::Game::StageKindMask kTest = KindBit(StageKind::kTest);

struct Snap {
    std::unique_ptr<StageCatalogueSnapshot> s{ new StageCatalogueSnapshot() };

    StageCatalogueSnapshotEntry& Add(const char* name, Dia::Game::StageKindMask kinds,
                                     std::initializer_list<const char*> tags = {}, const char* desc = "")
    {
        StageCatalogueSnapshotEntry& e = s->entries[s->entryCount++];
        e.name = StringCRC(name);
        e.kinds = kinds;
        for (const char* t : tags)
        {
            std::strncpy(e.tags[e.tagCount], t, StageCatalogueSnapshotEntry::kTagChars - 1);
            ++e.tagCount;
        }
        std::strncpy(e.description, desc, StageCatalogueSnapshotEntry::kDescriptionChars - 1);
        return e;
    }
    static void Result(StageCatalogueSnapshotEntry& e, Sev sev, const char* label = "")
    {
        e.severity = sev;
        std::strncpy(e.label, label, sizeof(e.label) - 1);
    }
};

// Showcase: Alpha; Tools: Beta, Both; Tests: Gamma, Both, Delta; Other: Plain.
std::unique_ptr<Snap> MakeSnap()
{
    std::unique_ptr<Snap> sn(new Snap());
    sn->Add("Alpha", kShow, { "showcase", "demo" }, "Pretty things");
    sn->Add("Beta", kTool, { "tool", "editor" }, "A tool");
    sn->Add("Gamma", kTest, { "test", "physics" }, "Physics check");
    sn->Add("Both", kTool | kTest, { "tool", "test", "physics" }, "Dual role");
    sn->Add("Delta", kTest, { "test" }, "Another");
    sn->Add("Plain", 0, { "misc" }, "Nothing special");
    return sn;
}

void Snapshot(BootMenuLogic& l, Snap& s) { l.SetSnapshot(*s.s); }

unsigned int TotalRows(const BootMenuLogic& l)
{
    unsigned int n = 0;
    for (unsigned int g = 0; g < l.GroupCount(); ++g) n += l.RowCount(g);
    return n;
}

int FindGroup(const BootMenuLogic& l, StageGroup g)
{
    for (unsigned int i = 0; i < l.GroupCount(); ++i)
    {
        BootMenuLogic::GroupInfo info;
        l.GetGroup(i, info);
        if (info.group == g) return static_cast<int>(i);
    }
    return -1;
}
}

TEST(BootMenuLogic, GroupOrderAndHiddenEmptyGroups)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    ASSERT_EQ(l->GroupCount(), 4u);
    const StageGroup order[] = { StageGroup::kShowcase, StageGroup::kTool, StageGroup::kTest, StageGroup::kOther };
    for (unsigned int i = 0; i < 4; ++i)
    {
        BootMenuLogic::GroupInfo g;
        ASSERT_TRUE(l->GetGroup(i, g));
        EXPECT_EQ(g.group, order[i]);
    }

    // No showcase and no other stages: both groups hidden.
    Snap s2;
    s2.Add("T1", kTool, { "tool" });
    s2.Add("X1", kTest, { "test" });
    l->SetSnapshot(*s2.s);
    ASSERT_EQ(l->GroupCount(), 2u);
    BootMenuLogic::GroupInfo g;
    l->GetGroup(0, g);
    EXPECT_EQ(g.group, StageGroup::kTool);
    l->GetGroup(1, g);
    EXPECT_EQ(g.group, StageGroup::kTest);
    EXPECT_FALSE(l->GetGroup(2, g));
}

TEST(BootMenuLogic, HeaderShowsStageCount)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    BootMenuLogic::GroupInfo g;
    ASSERT_TRUE(l->GetGroup(1, g));
    EXPECT_EQ(g.stageCount, 2u);
    EXPECT_STREQ(g.title, "Tools (2)");
    ASSERT_TRUE(l->GetGroup(0, g));
    EXPECT_STREQ(g.title, "Showcase (1)");
}

TEST(BootMenuLogic, TwoKindStageHasTwoRowsButOneStage)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    EXPECT_EQ(l->StageCount(), 6u);
    EXPECT_EQ(TotalRows(*l), 7u);   // Both counted in Tools and Tests
    unsigned int seen = 0;
    for (unsigned int g = 0; g < l->GroupCount(); ++g)
        for (unsigned int r = 0; r < l->RowCount(g); ++r)
        {
            BootMenuLogic::Row row;
            ASSERT_TRUE(l->GetRow(g, r, row));
            if (row.stage == StringCRC("Both")) ++seen;
        }
    EXPECT_EQ(seen, 2u);
}

TEST(BootMenuLogic, SelectionAndVisitedBelongToTheStage)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    EXPECT_FALSE(l->IsSelected(StringCRC("Both")));
    l->Launch(StringCRC("Both"));
    // Both rows of "Both" read the same state.
    for (unsigned int g = 0; g < l->GroupCount(); ++g)
        for (unsigned int r = 0; r < l->RowCount(g); ++r)
        {
            BootMenuLogic::Row row;
            l->GetRow(g, r, row);
            const bool isBoth = row.stage == StringCRC("Both");
            EXPECT_EQ(l->IsSelected(row.stage), isBoth);
            EXPECT_EQ(l->IsVisited(row.stage), isBoth);
        }
    EXPECT_EQ(l->VisitedCount(), 1u);
}

TEST(BootMenuLogic, ChipsAreAndedAcrossTags)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    auto chip = [&](const char* name) {
        for (unsigned int i = 0; i < l->ChipCount(); ++i)
            if (std::strcmp(l->ChipName(i), name) == 0) return static_cast<int>(i);
        return -1;
    };
    const int test = chip("test"), physics = chip("physics");
    ASSERT_GE(test, 0);
    ASSERT_GE(physics, 0);
    l->SetChipActive(static_cast<unsigned int>(test), true);
    EXPECT_EQ(l->GroupCount(), 2u);        // Tools (Both) and Tests
    EXPECT_EQ(TotalRows(*l), 4u);          // Both (Tools), Gamma, Both, Delta
    l->SetChipActive(static_cast<unsigned int>(physics), true);
    EXPECT_EQ(TotalRows(*l), 3u);          // Both (Tools), Gamma, Both (Delta lacks physics)
    l->SetChipActive(static_cast<unsigned int>(test), false);
    EXPECT_EQ(TotalRows(*l), 3u);          // physics only: same stages
    l->ClearFilters();
    EXPECT_EQ(TotalRows(*l), 7u);
}

TEST(BootMenuLogic, ChipOrderKindsFirstThenAlphabetical)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    ASSERT_EQ(l->ChipCount(), 7u);
    const char* expected[] = { "showcase", "tool", "test", "demo", "editor", "misc", "physics" };
    for (unsigned int i = 0; i < 7; ++i)
        EXPECT_STREQ(l->ChipName(i), expected[i]) << i;
}

TEST(BootMenuLogic, SearchMatchesNameAndDescriptionCaseInsensitive)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    l->SetSearch("ALPH");   // name
    EXPECT_EQ(TotalRows(*l), 1u);
    l->SetSearch("pHySiCs ch");   // description of Gamma
    EXPECT_EQ(TotalRows(*l), 1u);
    l->SetSearch("zzz");
    EXPECT_EQ(l->GroupCount(), 0u);
    l->SetSearch("");
    EXPECT_EQ(TotalRows(*l), 7u);
}

TEST(BootMenuLogic, SearchAndChipsCombine)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    for (unsigned int i = 0; i < l->ChipCount(); ++i)
        if (std::strcmp(l->ChipName(i), "tool") == 0) l->SetChipActive(i, true);
    l->SetSearch("dual");
    EXPECT_EQ(TotalRows(*l), 2u);   // Both (Tools + Tests rows)
    l->SetSearch("a tool");
    EXPECT_EQ(TotalRows(*l), 1u);   // Beta only
}

TEST(BootMenuLogic, BadgeOnlyOnTestStagesWithResultColours)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snap::Result(sn->s->entries[2], Sev::kOk, "PASSED");
    Snap::Result(sn->s->entries[3], Sev::kFail, "FAILED");
    Snap::Result(sn->s->entries[4], Sev::kWarn, "SLOW");
    Snap::Result(sn->s->entries[0], Sev::kOk, "PASSED");   // showcase stage: result present but no badge
    Snapshot(*l, *sn);

    BootMenuLogic::Badge b;
    ASSERT_TRUE(l->GetBadge(StringCRC("Gamma"), b));
    EXPECT_STREQ(b.label, "PASSED");
    EXPECT_EQ(b.colour, BootMenuLogic::BadgeColour::kGreen);
    ASSERT_TRUE(l->GetBadge(StringCRC("Both"), b));
    EXPECT_EQ(b.colour, BootMenuLogic::BadgeColour::kRed);
    ASSERT_TRUE(l->GetBadge(StringCRC("Delta"), b));
    EXPECT_EQ(b.colour, BootMenuLogic::BadgeColour::kAmber);
    EXPECT_STREQ(b.label, "SLOW");

    EXPECT_FALSE(l->GetBadge(StringCRC("Alpha"), b));
    EXPECT_FALSE(l->GetBadge(StringCRC("Beta"), b));
    EXPECT_FALSE(l->GetBadge(StringCRC("Plain"), b));
    EXPECT_FALSE(l->GetBadge(StringCRC("Unknown"), b));
}

TEST(BootMenuLogic, NoResultIsGreyPending)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    BootMenuLogic::Badge b;
    ASSERT_TRUE(l->GetBadge(StringCRC("Gamma"), b));
    EXPECT_STREQ(b.label, "PENDING");
    EXPECT_EQ(b.colour, BootMenuLogic::BadgeColour::kGrey);
    EXPECT_EQ(b.severity, Sev::kNone);
}

TEST(BootMenuLogic, TestsRollupCountsUniqueTestStages)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snap::Result(sn->s->entries[2], Sev::kOk);     // Gamma
    Snap::Result(sn->s->entries[3], Sev::kFail);   // Both (two rows, counted once)
    Snapshot(*l, *sn);
    l->MarkVisited(StringCRC("Delta"));            // visited, no result -> pending

    const BootMenuLogic::Counts c = l->TestRollup();
    EXPECT_EQ(c.passed, 1u);
    EXPECT_EQ(c.failed, 1u);
    EXPECT_EQ(c.pending, 1u);

    BootMenuLogic::GroupInfo g;
    ASSERT_TRUE(l->GetGroup(static_cast<unsigned int>(FindGroup(*l, StageGroup::kTest)), g));
    EXPECT_STREQ(g.title, "Tests (1 passed, 1 failed, 1 pending)");
}

TEST(BootMenuLogic, CollapseDefaultsAndPersistsAcrossSnapshot)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    EXPECT_FALSE(l->IsCollapsed(StageGroup::kShowcase));
    EXPECT_FALSE(l->IsCollapsed(StageGroup::kTool));
    EXPECT_TRUE(l->IsCollapsed(StageGroup::kTest));
    EXPECT_FALSE(l->IsCollapsed(StageGroup::kOther));

    l->ToggleCollapsed(StageGroup::kTest);
    l->ToggleCollapsed(StageGroup::kTool);
    sn->s->revision = 2;
    Snapshot(*l, *sn);
    EXPECT_FALSE(l->IsCollapsed(StageGroup::kTest));
    EXPECT_TRUE(l->IsCollapsed(StageGroup::kTool));
    BootMenuLogic::GroupInfo g;
    ASSERT_TRUE(l->GetGroup(1, g));
    EXPECT_TRUE(g.collapsed);
}

TEST(BootMenuLogic, NewRevisionKeepsSelectionVisitedAndFilters)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    l->Launch(StringCRC("Beta"));
    l->SetSearch("tool");
    for (unsigned int i = 0; i < l->ChipCount(); ++i)
        if (std::strcmp(l->ChipName(i), "editor") == 0) l->SetChipActive(i, true);
    const unsigned int rowsBefore = TotalRows(*l);

    sn->s->revision = 5;
    Snap::Result(sn->s->entries[2], Sev::kOk);
    Snapshot(*l, *sn);
    EXPECT_EQ(l->Revision(), 5u);
    EXPECT_TRUE(l->IsSelected(StringCRC("Beta")));
    EXPECT_TRUE(l->IsVisited(StringCRC("Beta")));
    EXPECT_STREQ(l->Search(), "tool");
    EXPECT_EQ(TotalRows(*l), rowsBefore);
    EXPECT_TRUE(l->HasActiveFilters());
}

TEST(BootMenuLogic, PresetSubRowsFollowStageRow)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    auto sn = MakeSnap();
    StageCatalogueSnapshotEntry& e = sn->s->entries[0];
    e.presetCount = 2;
    std::strcpy(e.presets[0].name, "fast");
    std::strcpy(e.presets[0].description, "Fast run");
    std::strcpy(e.presets[1].name, "slow");
    Snapshot(*l, *sn);

    EXPECT_EQ(l->RowCount(0), 3u);
    BootMenuLogic::Row row;
    ASSERT_TRUE(l->GetRow(0, 0, row));
    EXPECT_FALSE(row.isPreset);
    EXPECT_EQ(row.stage, StringCRC("Alpha"));
    EXPECT_EQ(row.preset.Value(), 0u);
    ASSERT_TRUE(l->GetRow(0, 1, row));
    EXPECT_TRUE(row.isPreset);
    EXPECT_EQ(row.stage, StringCRC("Alpha"));
    EXPECT_EQ(row.preset, StringCRC("fast"));
    EXPECT_STREQ(row.description, "Fast run");
    ASSERT_TRUE(l->GetRow(0, 2, row));
    EXPECT_EQ(row.preset, StringCRC("slow"));
    EXPECT_FALSE(l->GetRow(0, 3, row));
}

TEST(BootMenuLogic, FallbackFlatListWithoutSnapshot)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    const StringCRC stages[] = { StringCRC("A"), StringCRC("B"), StringCRC("C") };
    l->SetFallbackStages(stages, 3);
    EXPECT_TRUE(l->IsFallback());
    ASSERT_EQ(l->GroupCount(), 1u);
    BootMenuLogic::GroupInfo g;
    ASSERT_TRUE(l->GetGroup(0, g));
    EXPECT_STREQ(g.title, "Stages");
    ASSERT_EQ(l->RowCount(0), 3u);
    BootMenuLogic::Row row;
    ASSERT_TRUE(l->GetRow(0, 1, row));
    EXPECT_EQ(row.stage, StringCRC("B"));
    EXPECT_FALSE(row.isPreset);
    EXPECT_EQ(l->ChipCount(), 0u);
    EXPECT_FALSE(l->HasBadges());
    BootMenuLogic::Badge b;
    EXPECT_FALSE(l->GetBadge(StringCRC("A"), b));

    // A snapshot arriving later replaces the fallback list and keeps visited.
    l->MarkVisited(StringCRC("A"));
    auto sn = MakeSnap();
    Snapshot(*l, *sn);
    EXPECT_FALSE(l->IsFallback());
    EXPECT_EQ(l->StageCount(), 6u);
    EXPECT_TRUE(l->IsVisited(StringCRC("A")));
}

TEST(BootMenuLogic, EmptyCatalogueHasNoGroups)
{
    std::unique_ptr<BootMenuLogic> l(new BootMenuLogic());
    Snap s;
    l->SetSnapshot(*s.s);
    EXPECT_EQ(l->GroupCount(), 0u);
    EXPECT_EQ(l->StageCount(), 0u);
    EXPECT_EQ(l->ChipCount(), 0u);
}

TEST(BootMenuLogic, TitleDefaultsAndOverrides)
{
    EXPECT_STREQ(BootMenuLogic::ResolveTitle(nullptr), "Boot Menu");
    EXPECT_STREQ(BootMenuLogic::ResolveTitle(""), "Boot Menu");
    EXPECT_STREQ(BootMenuLogic::ResolveTitle("CoW Boot"), "CoW Boot");
}

TEST(BootMenuLogic, ProjectDefaultsAndOverrides)
{
    EXPECT_STREQ(BootMenuLogic::ResolveProject(nullptr), "");
    EXPECT_STREQ(BootMenuLogic::ResolveProject(""), "");
    EXPECT_STREQ(BootMenuLogic::ResolveProject("CoW v0.1"), "CoW v0.1");
}
