#include <gtest/gtest.h>

#include <Modules/StageCatalogueLoader.h>
#include <Testing/StageCatalogueFixture.h>

#include <filesystem>
#include <memory>

using namespace Cluiche::AppFlow;
using Dia::Core::StringCRC;
namespace fs = std::filesystem;

namespace {

fs::path Fixtures() { return Testing::FindStageCatalogueFixtures(__FILE__); }

std::string DiagamePath(const char* fixture) { return (Fixtures() / fixture / "game.diagame").string(); }

StageCatalogueResult LoadFixture(const char* fixture, StageCatalogue& cat) {
    return StageCatalogueLoader::Load(DiagamePath(fixture).c_str(), cat);
}

Json::Value Expected(const char* key) {
    Json::Value all;
    EXPECT_TRUE(Testing::ReadJsonFile(Fixtures() / "expected_catalogue.json", all));
    return all[key];
}

std::unique_ptr<StageCatalogue> LoadBasic() {
    auto cat = std::make_unique<StageCatalogue>();
    EXPECT_EQ(LoadFixture("basic", *cat), StageCatalogueResult::kOk);
    return cat;
}

void CompareToExpected(const char* fixture, const char* key, bool summaryOnly) {
    ASSERT_FALSE(Fixtures().empty());
    auto cat = std::make_unique<StageCatalogue>();
    const StageCatalogueResult r = LoadFixture(fixture, *cat);
    const Json::Value actual = Testing::ToExpectedJson(r, *cat, summaryOnly);
    EXPECT_EQ(actual, Expected(key)) << actual.toStyledString();
}

void CopyDir(const fs::path& from, const fs::path& to) {
    fs::create_directories(to);
    for (const auto& e : fs::directory_iterator(from)) {
        if (e.is_regular_file()) { fs::copy_file(e.path(), to / e.path().filename(), fs::copy_options::overwrite_existing); }
    }
}

} // namespace

// ---- Fixture equality with expected_catalogue.json (shared with the Python loader) ----

TEST(StageCatalogueLoader, BasicMatchesExpected)          { CompareToExpected("basic", "basic", false); }
TEST(StageCatalogueLoader, OverCapMatchesExpected)        { CompareToExpected("over_cap", "over_cap", true); }
TEST(StageCatalogueLoader, MalformedMatchesExpected)      { CompareToExpected("malformed", "malformed", false); }
TEST(StageCatalogueLoader, NoRootManifestMatchesExpected) { CompareToExpected("no_root_manifest", "no_root_manifest", false); }

TEST(StageCatalogueLoader, MissingDiagameMatchesExpected) {
    StageCatalogue* cat = new StageCatalogue();
    const StageCatalogueResult r = StageCatalogueLoader::Load((Fixtures() / "nonexistent" / "game.diagame").string().c_str(), *cat);
    EXPECT_EQ(Testing::ToExpectedJson(r, *cat), Expected("missing"));
    delete cat;
}

TEST(StageCatalogueLoader, OverCapKeepsFirst64InTransitionOrder) {
    auto cat = std::make_unique<StageCatalogue>();
    ASSERT_EQ(LoadFixture("over_cap", *cat), StageCatalogueResult::kOk);
    ASSERT_EQ(cat->Size(), StageCatalogue::kMaxStages);
    EXPECT_EQ(cat->DroppedCount(), 1u);
    EXPECT_EQ(cat->Find(StringCRC("Stage01")), 0);
    EXPECT_EQ(cat->Find(StringCRC("Stage64")), 63);
    EXPECT_EQ(cat->Find(StringCRC("Stage65")), -1);
}

TEST(StageCatalogueLoader, LoadClearsPreviousContentOnFailure) {
    auto cat = LoadBasic();
    ASSERT_GT(cat->Size(), 0u);
    EXPECT_EQ(LoadFixture("malformed", *cat), StageCatalogueResult::kMalformed);
    EXPECT_EQ(cat->Size(), 0u);
}

TEST(StageCatalogueLoader, EntryPathsAndUnimportedStage) {
    auto cat = LoadBasic();
    const int noImport = cat->Find(StringCRC("NoImport"));
    ASSERT_GE(noImport, 0);
    EXPECT_STREQ((*cat)[noImport].diastagePath.AsCStr(), "");
    const int rig = cat->Find(StringCRC("TestRig"));
    ASSERT_GE(rig, 0);
    EXPECT_NE(std::string((*cat)[rig].diastagePath.AsCStr()).find("TestRig.diastage"), std::string::npos);
}

TEST(StageCatalogueLoader, ImportFoundByUniqueFileNameWhenPathDiffers) {
    const fs::path tmp = fs::temp_directory_path() / "StageCatalogueFallbackTest";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    CopyDir(Fixtures() / "basic", tmp);
    // Move the stage files into a subdirectory the manifest imports do not mention.
    fs::create_directories(tmp / "sub");
    for (const auto& e : fs::directory_iterator(tmp)) {
        if (e.path().extension() == ".diastage") { fs::rename(e.path(), tmp / "sub" / e.path().filename()); }
    }
    auto cat = std::make_unique<StageCatalogue>();
    ASSERT_EQ(StageCatalogueLoader::Load((tmp / "game.diagame").string().c_str(), *cat), StageCatalogueResult::kOk);
    EXPECT_EQ(Testing::ToExpectedJson(StageCatalogueResult::kOk, *cat), Expected("basic"));
    fs::remove_all(tmp, ec);
}

TEST(StageCatalogueLoader, MissingImportedStageIsMalformed) {
    const fs::path tmp = fs::temp_directory_path() / "StageCatalogueMissingStageTest";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    CopyDir(Fixtures() / "basic", tmp);
    fs::remove(tmp / "Plain.diastage");
    auto cat = std::make_unique<StageCatalogue>();
    EXPECT_EQ(StageCatalogueLoader::Load((tmp / "game.diagame").string().c_str(), *cat), StageCatalogueResult::kMalformed);
    fs::remove_all(tmp, ec);
}

// ---- Queries ----

TEST(StageCatalogue, FindHitAndMiss) {
    auto cat = LoadBasic();
    EXPECT_EQ(cat->Find(StringCRC("TestRig")), 0);
    EXPECT_EQ(cat->Find(StringCRC("ShowcaseDemo")), 6);
    EXPECT_EQ(cat->Find(StringCRC("Nope")), -1);
    EXPECT_EQ(cat->Find(StringCRC("NotListed")), -1);  // omitted
}

TEST(StageCatalogue, KindsAreCanonicalMasks) {
    using namespace Dia::Game;
    auto cat = LoadBasic();
    EXPECT_EQ(cat->Kinds(0), KindBit(StageKind::kTest));
    EXPECT_EQ(cat->Kinds(1), 0);
    EXPECT_EQ(cat->Kinds(2), KindBit(StageKind::kShowcase) | KindBit(StageKind::kTest));
    EXPECT_EQ(cat->Kinds(3), KindBit(StageKind::kTool));
    EXPECT_EQ(cat->Kinds(4), 0);  // free-form tag only
    EXPECT_EQ(cat->Kinds(999), 0);
}

TEST(StageCatalogue, FilterAllTagsMustMatch) {
    auto cat = LoadBasic();
    StageCatalogueIndices out;
    const StringCRC both[] = { StringCRC("test"), StringCRC("showcase") };
    cat->Filter(both, 2, out);
    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], 2u);
}

TEST(StageCatalogue, FilterOneTag) {
    auto cat = LoadBasic();
    StageCatalogueIndices out;
    const StringCRC t[] = { StringCRC("test") };
    cat->Filter(t, 1, out);
    ASSERT_EQ(out.Size(), 2u);
    EXPECT_EQ(out[0], 0u);
    EXPECT_EQ(out[1], 2u);
}

TEST(StageCatalogue, FilterNoneMatching) {
    auto cat = LoadBasic();
    StageCatalogueIndices out;
    const StringCRC t[] = { StringCRC("tool"), StringCRC("showcase") };
    cat->Filter(t, 2, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(StageCatalogue, FilterEmptyReturnsAll) {
    auto cat = LoadBasic();
    StageCatalogueIndices out;
    cat->Filter(nullptr, 0, out);
    ASSERT_EQ(out.Size(), cat->Size());
    for (unsigned int i = 0; i < out.Size(); ++i) { EXPECT_EQ(out[i], i); }
}

TEST(StageCatalogue, FilterIsCaseSensitive) {
    auto cat = LoadBasic();
    StageCatalogueIndices out;
    const StringCRC t[] = { StringCRC("Test") };
    cat->Filter(t, 1, out);
    EXPECT_EQ(out.Size(), 0u);
}

TEST(StageCatalogue, FilterMatchesFreeFormTag) {
    auto cat = LoadBasic();
    StageCatalogueIndices out;
    const StringCRC t[] = { StringCRC("world-gen") };
    cat->Filter(t, 1, out);
    ASSERT_EQ(out.Size(), 1u);
    EXPECT_EQ(out[0], 3u);
}

TEST(StageCatalogue, GroupByKindTwoKindStageGetsTwoRows) {
    auto cat = LoadBasic();
    StageCatalogueRows rows;
    cat->GroupByKind(rows);
    unsigned int forShowTest = 0;
    for (unsigned int i = 0; i < rows.Size(); ++i) { if (rows[i].entryIndex == 2) { ++forShowTest; } }
    EXPECT_EQ(forShowTest, 2u);
}

TEST(StageCatalogue, GroupByKindFreeFormOnlyGoesToOther) {
    auto cat = LoadBasic();
    StageCatalogueRows rows;
    cat->GroupByKind(rows);
    bool found = false;
    for (unsigned int i = 0; i < rows.Size(); ++i) {
        if (rows[i].entryIndex == 4) { EXPECT_EQ(rows[i].group, StageGroup::kOther); found = true; }
    }
    EXPECT_TRUE(found);
}

TEST(StageCatalogue, GroupByKindOrderAndAscendingIndexWithinGroup) {
    auto cat = LoadBasic();
    StageCatalogueRows rows;
    cat->GroupByKind(rows);
    ASSERT_EQ(rows.Size(), 8u);
    for (unsigned int i = 1; i < rows.Size(); ++i) {
        const unsigned int g0 = static_cast<unsigned int>(rows[i - 1].group);
        const unsigned int g1 = static_cast<unsigned int>(rows[i].group);
        EXPECT_LE(g0, g1);
        if (g0 == g1) { EXPECT_LT(rows[i - 1].entryIndex, rows[i].entryIndex); }
    }
}

TEST(StageCatalogue, AddRejectsBeyondCapacity) {
    auto cat = std::make_unique<StageCatalogue>();
    StageCatalogueEntry e;
    for (unsigned int i = 0; i < StageCatalogue::kMaxStages; ++i) { EXPECT_TRUE(cat->Add(e)); }
    EXPECT_FALSE(cat->Add(e));
    EXPECT_EQ(cat->Size(), StageCatalogue::kMaxStages);
}
